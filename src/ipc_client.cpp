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
#include "debug.hpp"

using boost::asio::ip::tcp;

//
// public
ipc_client::ipc_client(struct ipc_config_s& config, boost::asio::io_service& _ipc_service):
    connection_(_ipc_service), resolver_(_ipc_service)
{
    //initialise internal data
    cfg = config;
    ipc_service = &_ipc_service;
    wstatus = IDLE;
    wcfg = {};

    //will block
    connect();
}

//can throw system_error excpetion (from boost) if IPC fails
void ipc_client::send_item(struct queue_node_s& data)
{
    dbg_1<<"sending node to master\n";
    connection_.wdata_type(dt_queue_node);
    connection_.wdata(data);
    connection_.async_write(boost::bind(&ipc_client::write_no_read, this,
        boost::asio::placeholders::error));

    //kick off ipc_service
    ipc_service->run();  //will block
    ipc_service->reset();
    dbg_1<<"completed ***\n";
}

struct queue_node_s ipc_client::get_item(void) throw(std::exception)
{
    dbg_1<<"requesting node from master\n";
    connection_.wdata_type(dt_instruction);
    connection_.wdata(ctrl_wnodes);
    connection_.async_write(boost::bind(&ipc_client::write_complete, this,
        boost::asio::placeholders::error));

    //kick off ipc_service
    ipc_service->run();  //will block
    ipc_service->reset();
    dbg_1<<"completed ***\n";

    struct queue_node_s data = {};
    if(!get_buffer.empty()) {
        dbg_1<<"data already present on queue\n";
        data = get_buffer.pop();
    } else {
        throw ipc_exception("get_buffer.data empty\n");
    }

    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    return data;
}

struct worker_config_s ipc_client::get_config(void)
{
    dbg<<"requesting registration config\n";
    connection_.wdata_type(dt_instruction);
    connection_.wdata(ctrl_wconfig);
    connection_.async_write(boost::bind(&ipc_client::write_complete, this,
        boost::asio::placeholders::error));

    //kick off ipc_service
    ipc_service->run();  //will block
    ipc_service->reset();
    dbg_1<<"completed ***\n";

    return wcfg;
}

//master requests status asynchronously (not referring to ipc). keep it
//updated here.
void ipc_client::set_status(worker_status_e& s)
{
    wstatus = s;
}

//same principle as set_status()
void ipc_client::set_capabilities(worker_capabilities_s& c)
{
    wcaps = c;
}

//
//private
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

    dbg_1<<"launching boost service\n";
    ipc_service->run();
    ipc_service->reset();
}

//placeholder function atm; used only for debug. In the future may launch
//the background thread which keeps the get_buffer filled and send_buffer drained
void ipc_client::handle_connected(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec)
        dbg<<"connected.\n";
    else
        throw ipc_exception("handle_connected() async_connect error: "+ec.message());
}

//generic async write return point (callback) - placeholder for debug only.
//registers generic read handler which processes replies/async master comms
void ipc_client::write_complete(boost::system::error_code ec) throw(std::exception)
{
    if(!ec) {
        dbg_1<<"sent data to master\n";
        connection_.async_read(boost::bind(&ipc_client::read_data, this,
            boost::asio::placeholders::error));
    } else {
        throw ipc_exception("failed to write to master: "+ec.message());
    }
}

//development debug version. threaded variant can afford for background
//thread to sleep on read callback (after send data), however atm we need
//the send_data call to not block indefinetley
void ipc_client::write_no_read(boost::system::error_code ec) throw(std::exception)
{
    if(!ec) {
        dbg_1<<"sent data to master. will not register read\n";
    } else {
        throw ipc_exception("failed to write to master: "+ec.message());
    }
}

//generic processing of data from master. data may be a reply to an earlier
//request or event/async communication such as getting worker status
void ipc_client::read_data(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        switch(connection_.rdata_type()) {
        case dt_instruction:
        {
            dbg<<"got ctrl instruction from master: "<<connection_.rdata<ctrl_instruction_e>()<<std::endl;
            process_instruction();
            break;
        }

        case dt_wconfig:
        {
            dbg<<"got config from master\n";
            wcfg = connection_.rdata<worker_config_s>();
            break;
        }

        case dt_queue_node:
        {
            dbg<<"got queue_node_s from master\n";
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

//stub
void ipc_client::process_instruction(void)
{
    //to do
}
