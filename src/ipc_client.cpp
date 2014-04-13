#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <chrono>
#include <boost/lockfree/queue.hpp>         //task queue
#include <boost/bind.hpp>                   //boost::bind
#include <boost/asio.hpp>                   //all ipc
#include <string>                           //to_string

#include "ipc_client.hpp"
#include "ipc_common.hpp"
#include "connection.hpp"

//Local defines
#define DEBUG 1

#if defined(DEBUG)
    #define dbg std::cout<<__FILE__<<"("<<__LINE__<<") "<<__func__<<": "
    #if DEBUG > 1
        #define dbg_1 std::cout<<__FILE__<<"("<<__LINE__<<") "<<__func__<<": "
    #else
        #define dbg_1 0 && std::cout
    #endif
#else
    #define dbg 0 && std::cout
    #define dbg_1 0 && std::cout
#endif

using boost::asio::ip::tcp;

ipc_client::ipc_client(struct ipc_config& config, boost::asio::io_service& _ipc_service):
    connection_(_ipc_service), resolver_(_ipc_service)
{
    cfg = config;

    //initialise internal data
    send_data = true;
    ipc_service = &_ipc_service;

    wstatus = IDLE;
    wcfg = {};

    connect();
}

ipc_client::~ipc_client(void)
{
}

//this thread runs in the background for the life of the object, handeling
//all the ipc and making sure node buffers are drained/filled etc
void ipc_client::connect(void) throw(std::exception)
{
    tcp::resolver::query query(cfg.master_address, MASTER_SERVICE_NAME);
    resolver_.async_resolve(query,
        [this](boost::system::error_code ec, tcp::resolver::iterator it)
        {
            if(!ec) {
                dbg_1<<"resolved master\n";
                dbg<<"connecting to: "<<it->endpoint()<<std::endl;
                boost::asio::async_connect(connection_.socket(), it,
                    boost::bind(&ipc_client::handle_connected, this,
                        boost::asio::placeholders::error));
            } else {
                throw ipc_exception("async_resolve error: "+ec.message());
            }
        });

    dbg<<"launching boost service\n";
    ipc_service->run();
    ipc_service->reset();
}

//connected to master, begin processing/scheduling tasks
void ipc_client::handle_connected(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec)
        dbg<<"connected.\n";
    else
        throw ipc_exception("handle_connected() async_connect error: "+ec.message());
}

void ipc_client::write_complete(boost::system::error_code ec) throw(std::exception)
{
    if(!ec) {
        dbg_1<<"sent request to master\n";
        connection_.async_read(boost::bind(&ipc_client::read_data, this,
            boost::asio::placeholders::error));
    } else {
        throw ipc_exception("process_task() failed to write to master: "+ec.message());
    }
}

void ipc_client::read_data(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        switch(connection_.rdata_type()) {
        case instruction:
        {
            dbg<<"got cnc instruction from master: "<<connection_.rdata<cnc_instruction>()<<std::endl;
            break;
        }

        case cnc_data:
        {
            dbg<<"read config from master\n";
            wcfg = connection_.rdata<worker_config>();
            break;
        }

        case queue_node:
        {
            dbg<<"read node from master\n";
            queue_node_s n = connection_.rdata<struct queue_node_s>();
            get_buffer.push(n);
            break;
        }

        default:
            throw ipc_exception("unknown data type recieved from master ("+std::to_string(connection_.rdata_type())+")\n");
        }


    } else {
        throw ipc_exception("get_data() boost error: "+ec.message());
    }
}

//send data to master - will keep calling itself until send_buffer.size < cfg.work_presend
void ipc_client::send_qnode(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        if(send_data) {
            send_data = false;

            dbg<<"sending node to master, send_buffer.size(): "<<send_buffer.size()<<std::endl;
            connection_.wdata_type(queue_node);
            connection_.wdata(send_buffer.pop());

            connection_.async_write(boost::bind(&ipc_client::send_qnode, this,
                boost::asio::placeholders::error));
        } else {
            dbg<<"sent node successfully\n";
            //reset for next run
            send_data = true;
        }
    } else {
        throw ipc_exception("send_to_master() failed to send queue_node to master: "+ec.message());
    }
}

//public intreface
//add item to send_buffer
void ipc_client::send_item(struct queue_node_s& data)
{
    send_buffer.push(data);
    dbg<<"added work item to queue, size is now: "<<send_buffer.size()<<std::endl;

    dbg<<"sending node to master\n";
    connection_.wdata_type(instruction);
    connection_.wdata(w_send_work);
    connection_.async_write(boost::bind(&ipc_client::send_qnode, this,
        boost::asio::placeholders::error));

    //kick off ipc_service
    ipc_service->run();  //will block
    ipc_service->reset();
    dbg_1<<"completed ***\n";
}

//public interface
struct queue_node_s ipc_client::get_item(void) throw(std::exception)
{
    dbg<<"requesting node from master\n";
    connection_.wdata_type(instruction);
    connection_.wdata(w_get_work);
    connection_.async_write(boost::bind(&ipc_client::write_complete, this,
        boost::asio::placeholders::error));

    //kick off ipc_service
    ipc_service->run();  //will block
    ipc_service->reset();
    dbg_1<<"completed ***\n";

    struct queue_node_s data = {};
    if(!get_buffer.empty()) {
        dbg<<"data present on queue\n";
        data = get_buffer.pop();
    } else {
        throw ipc_exception("get_buffer.data empty\n");
    }

    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    return data;
}

//public intreface
//request worker config from master. block until recieved
struct worker_config ipc_client::get_config(void)
{
    dbg<<"requesting registration config\n";
    connection_.wdata_type(instruction);
    connection_.wdata(w_register);
    connection_.async_write(boost::bind(&ipc_client::write_complete, this,
        boost::asio::placeholders::error));

    //kick off ipc_service
    ipc_service->run();  //will block
    ipc_service->reset();
    dbg_1<<"completed ***\n";

    return wcfg;
}

//public intreface
void ipc_client::set_status(worker_status& s)
{
    wstatus = s;
}
