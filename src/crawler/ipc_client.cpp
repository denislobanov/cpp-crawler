#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <system_error>

#include "ipc_client.hpp"

//Local defines
//~ #define DEBUG 1

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

ipc_client::~ipc_client(void) {}

ipc_client::ipc_client(struct ipc_config& config)
{
    cfg = config;
}

//dev only
void ipc_client::send_item(struct queue_node_s& data)
{
    queue_lock.lock();
    dev_queue.push(data);
    queue_lock.unlock();

    dbg<<"pushed data to queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
}

/*
bool ipc_client::send_item(struct queue_node_s& data) throw(std::system_error)
{
    //FIXME
    throw std::underflow_error("not yet implemented");
    return false;
}
*/
struct queue_node_s ipc_client::get_item(void) throw(std::underflow_error)
{
    struct queue_node_s data = {0};

    queue_lock.lock();
    if(dev_queue.empty()) {
        queue_lock.unlock();
        throw std::underflow_error("queue empty\n");

    } else {
        data = dev_queue.front();
        dev_queue.pop();
        queue_lock.unlock();
    }

    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    return data;
}

worker_intruction ipc_client::sync(struct sync_data& data) throw(std::underflow_error)
{
    throw std::underflow_error("not yet implemented");
    return NO_INST;
}

