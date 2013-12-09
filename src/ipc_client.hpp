#if !defined (IPC_CLIENT_H)
#define IPC_CLIENT_H

#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <boost/lockfree/spsc_queue.hpp>    //ringbuffer
#include <boost/asio.hpp>
//#include <memory> //unique_ptr

#include "ipc_common.hpp"
#include "page_data.hpp"

#define BUFFER_MAX_SIZE     2048
#define SERVICE_GRANUALITY  1       //seconds

/**
 * provided by process calling contructor, to configure ipc connection
 * user supplied/config file in production
 */
struct ipc_config {
    unsigned int work_prebuff;      //queue nodes to fetch when get_buffer < get_buffer_min
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
};

/**
 * background thread state
 */
enum thread_state_e {
    stop,
    run,
    connected,
    next_task,
    buffs_serv
};

struct node_buffer {
    std::atomic<unsigned int> size;
    boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> data;
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
     *  will throw underrun_exception if queue is empty
     */
    struct queue_node_s get_item(void) throw(std::underflow_error);

    /**
     * will block whilst waiting for config
     */
    struct worker_config get_config(void);

    private:
    struct ipc_config cfg;
    std::thread task_thread;
    worker_status status;
    struct worker_config config_from_master;
    struct ipc_message message;
    
    //controlling background thread
    boost::lockfree::queue<struct cnc_instruction> task_queue;
    std::atomic<thread_state_e> thread_state;

    //ipc
    boost::asio::io_service ipc_service;
    tcp::resolver resolver_;
    tcp::socket socket_;
    
    //internal work queues
    struct node_buffer get_buffer;
    struct node_buffer send_buffer;

    void ipc_thread(void);
    void noop(const boost::system::error_code& err);
    void get_from_master(const boost::system::error_code& err);
    void send_to_master(const boost::system::error_code& err);
    void process_from_master(const boost::system::error_code& err);
    void handle_resolved(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator);
    void handle_connected(const boost::system::error_code& err);
    void process_task(cnc_instruction& task);
    
};

#endif
