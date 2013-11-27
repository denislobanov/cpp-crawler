#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <system_error>
#include <boost/lockfree/spsc_queue.hpp>    //ringbuffer
#include <boost/lockfree/queue.hpp>         //task queue
#include <boost/asio.hpp>                   //all ipc
#include <unistd.h>                         //sleep()

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

using boost::asio::ip::tcp;

ipc_client::~ipc_client(void)
{
    //tell thread to shut down
    running = false;

    task_thread.join();
}

ipc_client::ipc_client(struct ipc_config& config)
{
    cfg = config;

    //init state machine
    running = false
    connected = false;
    next_task = true;

    //init internal tracking
    status = IDLE;
    config_from_master = {0};
    data = {0};

    task_thread = std::thread(ipc_thread);
    task_thread.detach();
}

void ipc_client::ipc_thread(void) throw(std::invalid_argument): resolver_(ipc_service), socket_(ipc_service)
{
    tcp::resolver::query query(cfg.master_address, MASTER_SERVICE_NAME, MASTER_SERVICE_PORT);
    resolver_.async_resolve(query,
        boost::bind(&ipc_client::handle_resolved, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));

    ipc_service.run();
    running = true;

    while(running) {
        if(connected) {
            //next_task prevents spamming of master and makes sure follow up
            //replies/reads occure in order
            if(next_task) {
                cnc_instruction task = {0};
                while(!task_queue.pop(task)) {
                    process_task(task);
                }
            }

            //dont fill buffers until we have a connection (long connection time can result in
            // > cfg.work_prebuff w_get_work tasks on queue)
            if(get_buffer_size < cfg.get_buffer_min) {
                dbg<<"adding "<<cfg.work_prebuff - get_buffer_size<<" w_get_work tasks to queue\n";
                
                for(unsigned int i = cfg.work_prebuff - get_buffer_size; i > 0; --i) {
                    cnc_instruction task = w_get_work;
                    task_queue.push(task);
                }
            }

            if(send_buffer_size > cfg.work_presend) {
                dbg<<"adding "<<send_buffer_size<<" w_send_work tasks to queue\n";

                for(unsigned int i = send_buffer_size; i > 0; --i) {
                    cnc_instruction task = w_send_work;
                    task_queue.push(task);
                }
            }
        }

        sleep(1);
    }
}

void ipc_client::process_task(cnc_instruction& task)
{
    struct ipc_message message = {0};
    switch(task) {
        case w_register:
        case w_get_work:
            message = {
                .type = instruction;
                .data.instruction = task;
            };

            boost::asio::async_write(socket_, message,
                boost::bind(&ipc_client::get_from_master, this,
                    boost::asio::placeholders::error));
            break;

        case w_send_work:
            message = {
                .type = instruction;
                .data.instruction = task;
            };

            boost::asio::async_write(socket_, message,
                boost::bind(&ipc_client::send_to_master, this,
                    boost::asio::placeholders::error));
            break;

        case m_send_status:
            message = {
                .type = cnc_data;
                .data.status = status;
            };

            boost::asio::async_write(socket_, message,
                boost::bind(&ipc_client::noop, this,
                    boost::asio::placeholders::error));
            break;

        default:
            throw std::invalid_argument("task queue contains non worker task! ("<<task.type<<")\n");
    }

    next_task = false;
}

//send a request to master. get request response
void ipc_client::get_from_master(const boost::system::error_code& err) throw(std::system_error)
{
    if(err)
        throw std::system_error("get_from_master: "<<err.message()<<std::endl);
    else
        boost::asio::async_read(socket_, data,
            boost::asio::transfer_all(sizeof(data)),
            boost::bind(&ipc_client::process_from_master, this,
                boost::asio::placeholders::error));
}

//got request response from master, process it
void ipc_client::process_from_master(const boost::system::error_code& err) throw(std::system_error)
{
    if(err)
        throw std::system_error("process_from_master: "<<err.message()<<std::endl);

    else {
        switch(data.type) {
        case instruction:
            dbg_1<<"got instruction from master ("<<data.data.instruction<<")\n";
            task_queue.push(data.data.instruction);
            break;

        case cnc_data:
            dbg<<"got config data from master\n";
            config_from_master = data.data.config;
            break;

        case queue_node:
        {
            dbg_2<<"get work queue node from master\n";
            get_buffer.push(data.data.node);
            ++get_buffer_size;

            //loop until get_buffer is filled to avoid stalling crawler
            if(get_buffer_size < cfg.work_prebuff) {
                dbg_2<<"asking for more work nodes ("<<get_buffer_size < cfg.work_prebuff<<")\n";
                ipc_message message = {
                    .type = instruction;
                    .data.instruction = w_get_work;
                };

                boost::asio::async_write(socket_, message,
                    boost::bind(&ipc_client::get_from_master, this,
                        boost::asio::placeholders::error));
            }
            break;
        }

        default:
            throw std::system_error("process_from_master: data has invalid type ("<<data.type<<")\n");
        }

        next_task = true;
    }
}

//send follow up data to master
void ipc_client::send_to_master(const boost::system::error_code& err) throw(std::system_error)
{
    if(err)
        throw std::system_error("send_to_master: "<<err.message()<<std::endl);

    else {
        struct ipc_message message = {
            .type = queue_node;
            .data.queue_node = {0};
        };

        //send_buffer do not try to completely drain the send_buffer as to
        //allow other tasks to be processed
        if(send_buffer_size > cfg.work_presend) {
            send_buffer.pop(message.data.queue_node);
            --send_buffer_size;
            boost::asio::async_write(socket_, message,
                boost::bind(&ipc_client::send_to_master, this
                    boost::asio::placeholders::error));
        } else {
            dbg<<"drained send_buffer\n";
            next_task = true;
        }
    }
}

//sent response to master, no follow up actions required (do next task)
void ipc_client::noop(const boost::system::error_code& err) throw(std::system_error)
{
    if(err)
        throw std::system_error("noop: "<<err.message()<<std::endl);
    else
        next_task = true;

    dbg<<"noop done\n";
}

//master address resolved, connect
void ipc_client::handle_resolved(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator) throw(std::invalid_argument)
{
    if(err)
        throw std::invalid_argument("handle_resolve: "<<err.message()<<std::endl);

    else
        boost::asio::async_connect(socket_, endpoint_iterator,
            boost::bind(&ipc_client::handle_connected, this,
                boost::asio::placeholders::error));

    dbg<<"resolved master address\n";
}

//connected to master, begin processing/scheduling tasks
void ipc_client::handle_connected(const boost::system::error_code& err) throw(std::system_error)
{
    if(err)
        throw std::system_error("handle_connect: "<<err.message()<<std::endl);
    else
        connected = true;

    dbg<<"connected.\n";
}

//public intreface
//add item to send_buffer
bool ipc_client::send_item(struct queue_node_s& data)
{
    ++send_buffer_size;
    return send_buffer.push(data);
}

//public interface
//get item from get_buffer
struct queue_node_s ipc_client::get_item(void) throw(std::underflow_error)
{
    struct queue_node_s data = {0};

    if(!get_buffer.pop(data))
        throw std::underflow_error("get_buff buffer queue empty\n");

    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    --get_buffer_size;
    return data;
}

//public intreface
//request worker config from master. block until recieved
struct worker_config ipc_client::get_config(void)
{
    cnc_instruction task = w_register;
    task_queue.push(task);

    //block whilst waiting for configs
    while(config_from_master == 0) {
        sleep(1);
    }

    return config_from_master;
}

void ipc_client::set_status(worker_status& s)
{
    status = s;
}
