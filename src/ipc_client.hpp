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

#define BUFFER_MAX_SIZE     2048
#define SERVICE_GRANUALITY  std::chrono::milliseconds(500)
/**
 * provided by process calling contructor, to configure ipc connection
 * user supplied/config file in production
 */
struct ipc_config {
    unsigned int get_buffer_min;    //min size of get_buffer before fetching data
    unsigned int work_presend;      //max size of send_buffer before draining
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
    stop,
    run,
    connected,
    next_task
};

struct node_buffer_s {
    std::atomic<unsigned int> size;
    std::queue<struct queue_node_s> data;
    std::mutex lock;
};

struct task_queue_s {
    std::atomic<unsigned int> size;
    std::queue<cnc_instruction> data;
    std::mutex lock;
};

using boost::asio::ip::tcp;

class ipc_client
{
    public:
    ipc_client(struct ipc_config& config);
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
    std::thread task_thread;
    worker_status status;
    struct worker_config config_from_master;
    struct ipc_message message;

    //controlling background thread
    struct task_queue_s task_queue;
    std::atomic<thread_state_e> thread_state;
    std::atomic<bool> synced;
    std::atomic<bool> got_config;

    //ipc
    boost::asio::io_service ipc_service;
    tcp::resolver resolver_;
    tcp::socket socket_;

    //internal work queues
    struct node_buffer_s get_buffer;
    struct node_buffer_s send_buffer;

    void handle_connected(const boost::system::error_code& ec) throw(std::exception);
    void ipc_thread(void) throw(std::exception);
    void process_task(cnc_instruction task) throw(std::exception);
    void send_to_master(const boost::system::error_code& ec) throw(std::exception);
    void read_from_master(const boost::system::error_code& ec) throw(std::exception);
    void test_hndlr(const boost::system::error_code& ec) throw(std::exception);
};

#endif
