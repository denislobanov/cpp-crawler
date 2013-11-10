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
    unsigned int get_buff;  //size to fill get_buffer (during syncs)
    std::string master_address;
};

struct sync_data {
    worker_status status;
    struct capabilities cap;
    struct worker_config config;
};

class ipc_client
{
    public:
    ipc_client(void);
    ipc_client(struct ipc_config& config);
    ~ipc_client(void);

    /**
     * send discovered/crawled pages to master, will throw system_error if IPC failed
     *  buffered variant writes to a fifo ringbuffer first, which is
     *  asyncronasly sycronised with master depending on config
     */
    //~ bool send_item(struct queue_node_s& data) throw(std::system_error);
    //dev only
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
     * syncronises worker to master.
     *  sync_data must be fully set to properly handle all requests from master
     *  send queue is fully (size at time of call) flushed to master
     *  config.get_buff worth of queue_node_s items retrieved from master
     *
     * returns worker_intstruction as from master or NO_INST
     * will throw system_error if IPC failed
     */
    worker_intruction sync(struct sync_data& data) throw(std::underflow_error);

    private:
    struct ipc_config cfg;
    boost::asio::io_service ipc_service;
    boost::asio::ip::tcp::socket* cnc_socket;       //bidirectional cnc
    boost::asio::ip::tcp::socket* get_socket;      //get data from master
    boost::asio::ip::tcp::socket* send_socket;      //send data to master

    boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> send_buffer;
    boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> get_buffer;
};

#endif
