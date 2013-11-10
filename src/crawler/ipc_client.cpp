#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <system_error>
#include <boost/lockfree/spsc_queue.hpp>    //ringbuffer
#include <boost/asio.hpp>

#include "ipc_client.hpp"
#include "ipc_common.hpp"

//Local defines
#define DEBUG 1

#if defined(DEBUG)
    #define dbg std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #if DEBUG > 1
        #define dbg_1 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_1 0 && std::cout
    #endif
#else
    #define dbg 0 && std::cout
    #define dbg_1 0 && std::cout
#endif

ipc_client::ipc_client(void) {}

ipc_client::~ipc_client(void)
{
    delete cnc_socket;
    delete get_socket;
    delete send_socket;
}

ipc_client::ipc_client(struct ipc_config& config)
{
    cfg = config;

    //establish connection to master
    boost::asio::ip::tcp::resolver resolver(ipc_service);

    //cnc
    boost::asio::ip::tcp::resolver::query cnc_query(cfg.master_address, CNC_SERVICE_NAME);
    boost::asio::ip::tcp::resolver::iterator cnc_endpoint = resolver.resolve(cnc_query);
    cnc_socket = new boost::asio::ip::tcp::socket(ipc_service);
    boost::asio::connect(cnc_socket, cnc_endpoint);

    //get
    boost::asio::ip::tcp::resolver::query get_query(cfg.master_address, TO_WORKER_SERVICE_NAME);
    boost::asio::ip::tcp::resolver::iterator get_endpoint = resolver.resolve(get_query);
    get_socket = new boost::asio::ip::tcp::socket(ipc_service);
    boost::asio::connect(get_socket, get_endpoint);

    //send
    boost::asio::ip::tcp::resolver::query send_query(cfg.master_address, FROM_WORKER_SERVICE_NAME);
    boost::asio::ip::tcp::resolver::iterator send_endpoint = resolver.resolve(send_query);
    send_socket = new boost::asio::ip::tcp::socket(ipc_service);
    boost::asio::connect(send_socket, send_endpoint);
}

bool ipc_client::send_item(struct queue_node_s& data) throw(std::system_error)
{
    return send_buffer.push(data);
}

struct queue_node_s ipc_client::get_item(void) throw(std::underflow_error)
{
    struct queue_node_s data = {0};

    if(!get_buff.pop(data))
        throw std::underflow_error("get_buff buffer queue empty\n");

    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    return data;
}

worker_intruction ipc_client::sync(struct sync_data& data) throw(std::underflow_error)
{
    
}

