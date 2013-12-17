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
    syncing = false;
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
    syncing = true;
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
        throw ipc_exception("async_connect error in handle except: "+ec.message());
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
                throw ipc_exception("async_resolve error: "+ec.message());
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

                    dbg<<"processing task: "<<task<<" ***\n";
                    process_task(task);
                }
            }

            if(!syncing) {
                //dont fill get_buffer until we have a connection (long connection
                //times can result in > cfg.get_buffer_min w_get_work tasks on queue)
                task_queue.lock.lock();
                if(get_buffer.size < cfg.get_buffer_min) {
                    syncing = true;
                    cnc_instruction task = w_get_work;
                    dbg<<"adding w_get_work ("<<task<<") task to queue\n";
                    task_queue.data.push(task);
                    ++task_queue.size;
                }

                //send_to_master drains queue down to work_presend
                if(send_buffer.size > cfg.work_presend) {
                    syncing = true;
                    cnc_instruction task = w_send_work;
                    dbg<<"adding w_send_work ("<<task<<") task to queue\n";
                    task_queue.data.push(task);
                    ++task_queue.size;
                }
                task_queue.lock.unlock();

                dbg_1<<"checked sync\n";
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
        throw ipc_exception("process_task() failed to send to master: "+ec.message());
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
        boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
            [this, task](boost::system::error_code ec, std::size_t)
            {
                if(!ec) {
                    dbg<<"send message to master type ["<<task<<"]\n";
                    boost::asio::async_read(socket_, boost::asio::buffer(&message, sizeof(message)),
                        boost::bind(&ipc_client::read_from_master, this,
                            boost::asio::placeholders::error));
                } else {
                    throw ipc_exception("process_task() failed to send to master: "+ec.message());
                }
            });
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
                    throw ipc_exception("process_task() failed to send m_send_status to master: "+ec.message());
                }
            });
        break;
    }

    default:
        throw ipc_exception("task queue contains unknown or non-worker task! ("+std::to_string(task)+")\n");
    }

    //kick off ipc_service
    ipc_service.run();  //will block
    ipc_service.reset();
    dbg_1<<"process_task completed ***\n";
}

//send data to master - will keep calling itself until send_buffer.size < cfg.work_presend
void ipc_client::send_to_master(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        if(!syncing) //in case of out-of-order calling of get/send sync tasks
            syncing = true;

        dbg_1<<"sent node to master\n";
        message = {
            .type = queue_node,
            .data = {}
        };

        //do not completely drain the send_buffer to allow other tasks to be
        //completed (its always getting filled anyway)
        if(send_buffer.size > cfg.work_presend) {
            dbg<<"send lock\n";
            send_buffer.lock.lock();
            message.data.node = send_buffer.data.front();
            send_buffer.data.pop();
            --send_buffer.size;
            send_buffer.lock.unlock();
            dbg<<"send unlock\n";

            dbg_1<<"sending node to master\n";
            boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
                boost::bind(&ipc_client::send_to_master, this,
                    boost::asio::placeholders::error));
        } else {
            dbg<<"drained send_buffer\n";
            thread_state = next_task;
            syncing = false;
        }
    } else {
        throw ipc_exception("send_to_master() failed to send queue_node to master: "+ec.message());
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

            thread_state = next_task;
            break;

        case cnc_data:
            dbg<<"got config data from master\n";
            config_from_master = message.data.config;

            got_config = true;
            thread_state = next_task;
            break;

        case queue_node:
        {
            dbg_1<<"got work queue node from master url=["<<message.data.node.url<<"] credit=["<<message.data.node.credit<<"]\n";
            if(!syncing) //in case of out-of-order calling of get/send sync tasks
                syncing = true;

            get_buffer.lock.lock();
            dbg<<"buffer locked\n";
            get_buffer.data.push(message.data.node);
            dbg<<"data pushed\n";
            ++get_buffer.size;
            dbg<<"size ++\n";
            get_buffer.lock.unlock();
            dbg<<"done\n";

            //loop until get_buffer is filled to avoid stalling crawler
            if(get_buffer.size < cfg.get_buffer_min) {
                dbg_1<<"waiting for more work nodes ("<<get_buffer.size<<" < "<<cfg.get_buffer_min<<")\n";
                boost::asio::async_read(socket_,boost::asio::buffer(&message, sizeof(message)),
                    boost::bind(&ipc_client::read_from_master, this,
                        boost::asio::placeholders::error));
            } else {
                dbg<<"finished getting work nodes from master\n";
                thread_state = next_task;
                syncing = false;
            }
            break;
        }

        default:
            throw ipc_exception("read_from_master: message has invalid type ("+std::to_string(message.type)+")");
        }
    } else {
        throw ipc_exception("read_from_master boost error: "+ec.message());
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

    dbg<<"added work item to queue, size is now: "<<send_buffer.size<<std::endl;
}

//public interface
//get item from get_buffer, will block if buffer is empty until it has been
//filled by task_thread. Will throw exception if blocked for t seconds
struct queue_node_s ipc_client::get_item(unsigned int t) throw(std::exception)
{
    unsigned int dt = 0;
    struct queue_node_s data = {};

    //timed block if buffer is empty
    while(get_buffer.size == 0 && dt++ < t)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    if(get_buffer.size > 0) {
        
        dbg<<"data present on queue\n";
        get_buffer.lock.lock();
        data = get_buffer.data.front();
        get_buffer.data.pop();
        --get_buffer.size;
        get_buffer.lock.unlock();
    } else {
        throw ipc_exception("timout: get_buffer.data empty\n");
    }

    dbg<<"returning data from queue [credit: "<<data.credit<<" url: "<<data.url<<"]\n";
    return data;
}

//non-blocking implementation, throws exception if buffer is empty
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
        throw ipc_exception("get_buffer.data empty\n");
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
