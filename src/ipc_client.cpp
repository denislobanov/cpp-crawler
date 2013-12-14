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

//Local defines
#define DEBUG 2

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
    //internal tracking of task_thread
    cfg = config;
    thread_state = stop;
    synced = false;
    got_config = false;

    //init internal tracking of worker
    status = IDLE;
    config_from_master = {};
    message = {};

    //internal buffers
    get_buffer.size = 0;
    send_buffer.size = 0;
    task_queue.size = 0;

    //launch background service thread
    task_thread = std::thread(&ipc_client::ipc_thread, this);
    //~ task_thread.detach();
    dbg_1<<"task_thread detatched\n";
}

ipc_client::~ipc_client(void)
{
    //tell thread to shut down
    synced = true;
    thread_state = stop;
    task_thread.join();
}

//connected to master, begin processing/scheduling tasks
void ipc_client::handle_connected(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        thread_state = next_task;
        dbg<<"thread_state: "<<thread_state<<std::endl;
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
    tcp::resolver::query query(cfg.master_address, MASTER_SERVICE_NAME);
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
    thread_state = run;
    ipc_service.run();
    ipc_service.reset();

    while(thread_state >= run) {
        if(thread_state >= connected) {
            //prevents spamming of master and makes sure follow up replies/reads occure in order
            if(thread_state == next_task) {
                while(task_queue.size) {
                    thread_state = connected; //i.e. next_task == false
                    
                    task_queue.lock.lock();
                    cnc_instruction task = task_queue.data.front();
                    task_queue.data.pop();
                    --task_queue.size;
                    task_queue.lock.unlock();
                    
                    dbg<<"processing task: "<<task<<std::endl;
                    process_task(task);
                }
            }

            if(!synced) {
                synced = true;

                //dont fill get_buffer until we have a connection (long connection
                //times can result in > cfg.get_buffer_min w_get_work tasks on queue)
                task_queue.lock.lock();
                if(get_buffer.size < cfg.get_buffer_min) {
                    cnc_instruction task = w_get_work;
                    dbg<<"adding w_get_work ("<<task<<") task to queue\n";
                    task_queue.data.push(task);
                    ++task_queue.size;
                }

                //send_to_master drains queue down to work_presend
                if(send_buffer.size > cfg.work_presend) {
                    cnc_instruction task = w_send_work;
                    dbg<<"adding w_send_work ("<<task<<") task to queue\n";
                    task_queue.data.push(task);
                    ++task_queue.size;
                }
                task_queue.lock.unlock();
            }
            dbg<<"::\n";
        }

        std::this_thread::sleep_for(SERVICE_GRANUALITY);
    }
}

void ipc_client::test_hndlr(const boost::system::error_code& ec) throw(std::exception)
{
    dbg<<"aosenuthao\n\n";
    if(!ec) {
        dbg<<"send message to master\n";
        boost::asio::async_read(socket_, boost::asio::buffer(&message, sizeof(message)),
            boost::bind(&ipc_client::send_to_master, this,
                boost::asio::placeholders::error));
    } else {
        ipc_exception e("process_task() failed to send to master: "+ec.message());
        throw e;
    }
}

//there must not be any outstanding reads when this function enters
void ipc_client::process_task(cnc_instruction task) throw(std::exception)
{
    switch(task) {
    case w_register:
    case w_get_work:
    {
        message.type = instruction;
        message.data.instruction = task;

        dbg<<"sending data to master size "<<sizeof(message)<<"\n";
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
        dbg<<"sent data\n";
        break;
    }

    case w_send_work:
        message.type = instruction;
        message.data.instruction = task;

        //dont use lambda here due to send_to_master complexity
        boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
            boost::bind(&ipc_client::send_to_master, this,
                boost::asio::placeholders::error));
        break;

    case m_send_status:
    {
        message.type = instruction;
        message.data.instruction = task;

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

    //kick off ipc_service
    ipc_service.run();  //will block
    ipc_service.reset();
}

//send data to master - will keep calling itself until send_buffer.size < cfg.work_presend
void ipc_client::send_to_master(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        dbg_1<<"sent node to master\n";
        message = {
            .type = queue_node,
            .data = {}
        };

        //do not completely drain the send_buffer to allow other tasks to be
        //completed (its always getting filled anyway)
        if(send_buffer.size > cfg.work_presend) {
            send_buffer.lock.lock();
            message.data.node = send_buffer.data.front();
            send_buffer.data.pop();
            --send_buffer.size;
            send_buffer.lock.unlock();

            dbg_1<<"sending node to master\n";
            boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
                boost::bind(&ipc_client::send_to_master, this,
                    boost::asio::placeholders::error));
        } else {
            dbg<<"drained send_buffer\n";
            thread_state = next_task;
            synced = false;
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

            task_queue.lock.lock();
            ++task_queue.size;
            task_queue.data.push(message.data.instruction);
            task_queue.lock.unlock();
            break;

        case cnc_data:
            dbg<<"got config data from master\n";
            config_from_master = message.data.config;
            got_config = true;
            break;

        case queue_node:
        {
            dbg_1<<"get work queue node from master\n";
            get_buffer.lock.lock();
            get_buffer.data.push(message.data.node);
            ++get_buffer.size;
            get_buffer.lock.unlock();

            //loop until get_buffer is filled to avoid stalling crawler
            if(get_buffer.size < cfg.get_buffer_min) {
                dbg_1<<"asking for more work nodes ("<<get_buffer.size<<" < "<<cfg.get_buffer_min<<")\n";
                message.type = instruction;
                message.data.instruction = w_get_work;

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
void ipc_client::send_item(struct queue_node_s& data)
{
    send_buffer.lock.lock();
    send_buffer.data.push(data);
    ++send_buffer.size;
    send_buffer.lock.unlock();
}

//public interface
//get item from get_buffer
struct queue_node_s ipc_client::get_item(void) throw(std::exception)
{
    struct queue_node_s data = {};

    if(get_buffer.size > 0) {
        dbg<<"data present on queue\n";
        get_buffer.lock.lock();
        data = get_buffer.data.front();
        get_buffer.data.pop();
        --get_buffer.size;
        get_buffer.lock.unlock();
    } else {
        ipc_exception e("get_buffer.data empty\n");
        throw e;
    }
    
    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    return data;
}

//public intreface
//request worker config from master. block until recieved
struct worker_config ipc_client::get_config(void)
{
    cnc_instruction task = w_register;
    task_queue.lock.lock();
    ++task_queue.size;
    task_queue.data.push(task);
    task_queue.lock.unlock();
    dbg<<"added w_register task to queue\n";

    //block whilst waiting for master to send config
    while(!got_config)
        std::this_thread::sleep_for(SERVICE_GRANUALITY);

    //reset for next run
    got_config = false;
    return config_from_master;
}

//public intreface
void ipc_client::set_status(worker_status& s)
{
    status = s;
}
