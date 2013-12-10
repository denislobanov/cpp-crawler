#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <boost/lockfree/spsc_queue.hpp>    //ringbuffer
#include <boost/lockfree/queue.hpp>         //task queue
#include <boost/bind.hpp>                   //boost::bind
#include <boost/asio.hpp>                   //all ipc
#include <unistd.h>                         //sleep()
#include <string>                           //to_string

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

ipc_client::ipc_client(struct ipc_config& config): resolver_(ipc_service), socket_(ipc_service)
{
    cfg = config;
    thread_state = stop;

    //init internal tracking of worker
    status = IDLE;
    config_from_master = {};
    message = {};

    //launch background service thread
    task_thread = std::thread(&ipc_client::ipc_thread, this);
    task_thread.detach();
}

ipc_client::~ipc_client(void)
{
    //tell thread to shut down
    thread_state = stop;
    task_thread.join();
}

//connected to master, begin processing/scheduling tasks
void ipc_client::handle_connected(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        thread_state = connected;
        dbg<<"connected.\n";
    } else {
        ipc_exception e("async_connect error in handle except: "+ec.message());
        throw e;
    }
}

//this thread runs in the background for the life of the object, handeling
//all the ipc and making sure node buffers are drained/filled etc
void ipc_client::ipc_thread(void) throw(std::exception)
{
    tcp::resolver::query query(tcp::v4(), cfg.master_address, MASTER_SERVICE_NAME);
    resolver_.async_resolve(query,
        [this](boost::system::error_code ec, tcp::resolver::iterator it)
        {
            if(!ec) {
                dbg_1<<"resolved master\n";
                dbg<<"connecting to: "<<it->endpoint()<<std::endl;
                boost::asio::async_connect(socket_, it,
                    boost::bind(&ipc_client::handle_connected, this,
                        boost::asio::placeholders::error));
            } else {
                ipc_exception e("async_resolve error: "+ec.message());
                throw e;
            }
        });

    dbg<<"launching boost service\n";
    ipc_service.run();
    thread_state = run;

    while(thread_state >= run) {
        if(thread_state >= connected) {
            //prevents spamming of master and makes sure follow up replies/reads occure in order
            if(thread_state == next_task) {
                cnc_instruction task = {};
                while(!task_queue.pop(task)) {
                    process_task(task);
                }
            }

            //TEST: this may need to be a seperate bool ctrl
            if(thread_state < buffs_serv) {
                thread_state = buffs_serv;

                //copy current size as other threads will always modify it
                unsigned int s = get_buffer.size;

                //dont fill get_buffer until we have a connection (long connection
                //times can result in > cfg.work_prebuff w_get_work tasks on queue)
                if(s < cfg.get_buffer_min) {
                    dbg<<"adding w_get_work task to queue\n";
                    cnc_instruction task = w_get_work;
                    task_queue.push(task);
                }

                //send_to_master drains queue down to work_presend
                if(send_buffer.size > cfg.work_presend) {
                    dbg<<"adding w_send_work task to queue\n";
                    cnc_instruction task = w_send_work;
                    task_queue.push(task);
                }
            }
        }

        sleep(SERVICE_GRANUALITY);
    }
}

//there must not be any outstanding reads when this function enters
void ipc_client::process_task(cnc_instruction task) throw(std::exception)
{
    switch(task) {
    case w_register:
    case w_get_work:
    {
        message = {
            .type = instruction,
            .data.instruction = task
        };
        
        boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
            [this, task](boost::system::error_code ec, std::size_t)
            {
                if(!ec) {
                    dbg<<"send message to master type ["<<task<<"]\n";
                    boost::asio::async_read(socket_, boost::asio::buffer(&message, sizeof(message)),
                        boost::asio::transfer_all(),
                        boost::bind(&ipc_client::read_from_master, this,
                            boost::asio::placeholders::error));
                } else {
                    ipc_exception e("process_task() failed to send to master: "+ec.message());
                    throw e;
                }
            });
        break;
    }

    case w_send_work:
        message = {
            .type = instruction,
            .data.instruction = task
        };

        //dont use lambda here due to send_to_master complexity
        boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
            boost::bind(&ipc_client::send_to_master, this,
                boost::asio::placeholders::error));
        break;

    case m_send_status:
    {
        message = {
            .type = cnc_data,
            .data.status = status
        };

        boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
            [this, task](boost::system::error_code ec, std::size_t)
            {
                if(!ec) {
                    dbg<<"send message to master type ["<<task<<"]\n";
                    thread_state = next_task;
                } else {
                    ipc_exception e("process_task() failed to send m_send_status to master: "+ec.message());
                    throw e;
                }
            });
        break;
    }

    default:
    {
        ipc_exception e("task queue contains unknown or non-worker task! ("+std::to_string(task)+")\n");
        throw e;
    }
    }

    thread_state = connected; //i.e. next_task == false
}

//send data to master - will keep calling itself until send_buffer.size < cfg.work_presend
void ipc_client::send_to_master(const boost::system::error_code& ec, std::size_t) throw(std::exception)
{
    if(!ec) {
        dbg_1<<"sending node to master\n";
        message = {
            .type = queue_node,
            .data.node = {}
        };

        //do not completely drain the send_buffer to allow other tasks to be
        //completed (its always getting filled anyway)
        if(send_buffer.size > cfg.work_presend) {
            send_buffer.data.pop(message.data.node);
            --send_buffer.size;
            boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
                boost::bind(&ipc_client::send_to_master, this,
                    boost::asio::placeholders::error));
        } else {
            dbg<<"drained send_buffer\n";
            thread_state = next_task;
        }
    } else {
        ipc_exception e("send_to_master() failed to send queue_node to master: "+ec.message());
        throw e;
    }
}

//got request response from master, process it
void ipc_client::read_from_master(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        switch(message.type) {
        case instruction:
            dbg_1<<"got instruction from master ("+std::to_string(message.data.instruction)+")\n";
            task_queue.push(message.data.instruction);
            break;

        case cnc_data:
            dbg<<"got config data from master\n";
            config_from_master = reinterpret_cast<struct worker_config&>(message.data.config);
            break;

        case queue_node:
        {
            dbg_1<<"get work queue node from master\n";
            get_buffer.data.push(reinterpret_cast<struct queue_node_s&>(message.data.node));
            ++get_buffer.size;

            //loop until get_buffer is filled to avoid stalling crawler
            if(get_buffer.size < cfg.work_prebuff) {
                dbg_1<<"asking for more work nodes ("<<get_buffer.size<<" < "<<cfg.work_prebuff<<")\n";
                message = {
                    .type = instruction,
                    .data.instruction = w_get_work
                };

                boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
                    [this](boost::system::error_code ec, std::size_t)
                    {
                        if(!ec) {
                            dbg_1<<"lambda read_from_master wrote w_get_work\n";
                            boost::asio::async_read(socket_,boost::asio::buffer(&message, sizeof(message)),
                                boost::bind(&ipc_client::read_from_master, this,
                                    boost::asio::placeholders::error));
                        } else {
                            ipc_exception e("lambda read_from_master failed\
                                to bind async_read - read_from_master: "+ec.message());
                                throw e;
                        }
                    });
            }
            break;
        }

        default:
            ipc_exception e("read_from_master: message has invalid type ("+std::to_string(message.type)+")");
            throw e;
        }

        thread_state = next_task;
    } else {
        ipc_exception e("read_from_master boost error: "+ec.message());
            throw e;
    }
}

//public intreface
//add item to send_buffer
bool ipc_client::send_item(struct queue_node_s& data)
{
    ++send_buffer.size;
    bool r = send_buffer.data.push(data);
    if(!r) {
        ipc_exception e("failed to push data to send_buffer");
        throw e;
    }

    return r;
}

//public interface
//get item from get_buffer
struct queue_node_s ipc_client::get_item(void) throw(std::exception)
{
    struct queue_node_s data = {};

    if(!get_buffer.data.pop(data)) {
        ipc_exception e("get_buffer.data empty\n");
        throw e;
    }

    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    --get_buffer.size;
    return data;
}

//public intreface
//request worker config from master. block until recieved
struct worker_config ipc_client::get_config(void)
{
    cnc_instruction task = w_register;
    task_queue.push(task);

    //FIXME: block whilst waiting for configs
    sleep(1);

    return config_from_master;
}

void ipc_client::set_status(worker_status& s)
{
    status = s;
}
