#if !defined (IPC_CLIENT_H)
#define IPC_CLIENT_H

#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <chrono>
#include <boost/lockfree/queue.hpp>
#include <boost/asio.hpp>

#include "ipc_common.hpp"
#include "page_data.hpp"
#include "connection.hpp"

#define BUFFER_MAX_SIZE     2048
#define SERVICE_GRANUALITY  std::chrono::milliseconds(500)
/**
 * provided by process calling contructor, to configure ipc connection
 * user supplied/config file in production
 */
struct ipc_config {
    unsigned int gbuff_min;         //min size of get_buffer before fetching data
    unsigned int sbuff_max;         //max size of send_buffer before draining
    unsigned int sc;                //nodes to send to fill/drain buffer
    std::string master_address;
};

/**
 * generic exception interface to ipc_client
 */
struct ipc_exception: std::exception {
    std::string message;
    const char* what() const noexcept
    {
        return message.c_str();
    }
    ipc_exception(std::string s): message(s) {};
};

/**
 * background thread state
 */
enum thread_state_e {
    st_stop,
    st_run,
    st_connected,
    st_next_task,
    st_processing
};

template<typename T> class mpmc_queue {
    public:
    mpmc_queue(void)
    {
        size_ = 0;
    }

    ~mpmc_queue(void) {}

    void push(T t)
    {
        lock.lock();
        data.push(t);
        ++size_;
        lock.unlock();
    }

    T pop(void)
    {
        T t;
        lock.lock();
        t = data.front();
        data.pop();
        --size_;
        lock.unlock();

        return t;
    }

    std::size_t size(void)
    {
        return size_;
    }

    bool empty(void)
    {
        return data.empty();
    }

    private:
    std::atomic<std::size_t> size_;
    std::queue<T> data;
    std::mutex lock;
};

using boost::asio::ip::tcp;

class ipc_client
{
    public:
    ipc_client(struct ipc_config& config, boost::asio::io_service& _ipc_service);
    ~ipc_client(void);

    /**
     * send discovered/crawled pages to master, will throw system_error if IPC failed
     *  buffered variant writes to a fifo ringbuffer first, which is
     *  asyncronasly sycronised with master depending on config
     */
    void send_item(struct queue_node_s& data);

    /**
     * gets a new page to crawl from get queue. queue is filled up to
     * config.get_buff on sync
     *
     * development version uses internal (non ipc) work queue
     *
     *  will throw exception if queue is empty
     */
    struct queue_node_s get_item(void) throw(std::exception);

    /**
     * will block whilst waiting for config
     */
    struct worker_config get_config(void);

    void set_status(worker_status& s);

    private:
    struct ipc_config cfg;
    worker_status wstatus;
    struct worker_config wcfg;

    //controlling background thread
    std::atomic<bool> send_data;

    //ipc
    connection connection_;
    boost::asio::io_service* ipc_service;
    tcp::resolver resolver_;

    //internal work queues
    mpmc_queue<struct queue_node_s> get_buffer;
    mpmc_queue<struct queue_node_s> send_buffer;

    void connect(void) throw(std::exception);
    void handle_connected(const boost::system::error_code& ec) throw(std::exception);
    void process_task(cnc_instruction task) throw(std::exception);
    void read_data(const boost::system::error_code& ec) throw(std::exception);
    void send_qnode(const boost::system::error_code& ec) throw(std::exception);
    void write_complete(boost::system::error_code ec) throw(std::exception);
};

#endif
