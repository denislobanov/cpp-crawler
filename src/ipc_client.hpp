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
struct ipc_config_s {
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

/**
 * Internal implementation of a multiple producer multiple consumer queue
 *
 * This was necessary as boost locking/size tracking was not functioning
 * properly.
 */
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
    ipc_client(struct ipc_config_s& config, boost::asio::io_service& _ipc_service);

    /**
     * Used to send discovered/crawled pages to master.
     * 
     * Add item to send_buffer and immediately send. A thread will poll the
     * send_buffer in the future and decide when to batch send.
     *
     * Will block whilst sending data.
     * Will throw system_error exception if IPC failed.
     */
    void send_item(struct queue_node_s& data);

    /**
     * Requests one node from master and delivers to caller.
     * In the future function will immidiately return data off of the get_buffer
     * if any is present, and block only if its fully drained. A background
     * queue will (try to) keep this buffer filled.
     *
     * Will block whilst waiting for data from master.
     * Will throw exception if queue is empty or IPC has failed.
     */
    struct queue_node_s get_item(void) throw(std::exception);

    /**
     * Gets configuration structure from master. Should be used for subsequest
     * polls to make sure configuration is up-to-date.
     *
     * In the future only the first call will block and the master may asynchronously
     * notify the client background ipc thread of configuration changes to fetch.
     * The internal monitoring process can then handle exception/return false
     * for unchanged configuration information.
     *
     * Currently will block on each call. Each call results in a worker_config_s
     * data transfer.
     *
     * Will block.
     * Will throw system_error exception if IPC has failed.
     */
    struct worker_config_s get_config(void);

    /**
     * Used to set worker status as reported to master.
     *
     * Does not block, will not throw exception.
     */
    void set_status(worker_status_e& s);

    /**
     * Used to set worker capabilities as reported to master.
     *
     * Will not block or throw exception.
     */
    void set_capabilities(worker_capabilities_s& c);

    private:
    struct ipc_config_s cfg;
    worker_status_e wstatus;
    struct worker_config_s wcfg;
    struct worker_capabilities_s wcaps;

    //ipc
    connection connection_;
    boost::asio::io_service* ipc_service;
    tcp::resolver resolver_;

    //internal work queues
    mpmc_queue<struct queue_node_s> get_buffer;
    mpmc_queue<struct queue_node_s> send_buffer;

    void connect(void) throw(std::exception);
    void handle_connected(const boost::system::error_code& ec) throw(std::exception);
    void write_complete(boost::system::error_code ec) throw(std::exception);
    void write_no_read(boost::system::error_code ec) throw(std::exception);
    void read_data(const boost::system::error_code& ec) throw(std::exception);
    void process_instruction(void);
};

#endif
