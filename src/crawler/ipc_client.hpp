#if !defined (IPC_CLIENT_H)
#define IPC_CLIENT_H

#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <system_error>
#include <boost/lockfree/spsc_queue.hpp>    //ringbuffer
#include <boost/asio.hpp>

#include "ipc_common.hpp"
#include "page_data.hpp"

#define BUFFER_MAX_SIZE     2048

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
    std::thread* ipc_thread_h;
    worker_status status;
    struct worker_config config_from_master;
    struct ipc_message data;
    
    //state variables for controlling background thread
    std::atomic<bool> connected;
    std::atomic<bool> running;
    std::atomic<bool> next_task;
    std::atomic<unsigned int> get_buffer_size;
    std::atomic<unsigned int> send_buffer_size;

    //ipc
    boost::asio::io_service ipc_service;
    tcp::resolver resolver_;
    tcp::socket socket_;

    //buffers
    boost::lockfree::queue<struct cnc_instruction> task_queue;
    boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> send_buffer;
    boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> get_buffer;

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
