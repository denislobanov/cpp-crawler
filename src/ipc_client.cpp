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

ipc_client::ipc_client(struct ipc_config& config):
    resolver_(ipc_service), connection_(ipc_service)
{
    cfg = config;

    //initialise internal data
    thread_state = stop;
    syncing = false;
    got_config = false;
    
    status = IDLE;
    worker_cfg = {};
    
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
                if(get_buffer.size < cfg.gbuff_min) {
                    syncing = true;
                    cnc_instruction task = w_get_work;
                    dbg_1<<"adding w_get_work ("<<task<<") task to queue\n";
                    task_queue.data.push(task);
                    ++task_queue.size;
                }

                //send_to_master drains queue down to work_presend
                if(send_buffer.size > cfg.sbuff_max) {
                    syncing = true;
                    cnc_instruction task = w_send_work;
                    dbg_1<<"adding w_send_work ("<<task<<") task to queue\n";
                    task_queue.data.push(task);
                    ++task_queue.size;
                }
                task_queue.lock.unlock();

                dbg_1<<"checked sync\n";
            }
        }

        std::this_thread::sleep_for(SERVICE_GRANUALITY);
    }
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

//there must not be any outstanding reads when this function enters
void ipc_client::process_task(cnc_instruction task) throw(std::exception)
{
    ipc_cnc = task;

    switch(task) {
    case w_register:
        connection_.data_type(instruction);
        connection_.data(task);
        connection_.async_write(
            [this, task](boost::system::error_code ec, std::size_t)
            {
                if(!ec) {
                    dbg_1<<"sent registration request to master\n";
                    connection_.async_read(boost::bind(&ipc_client::get_wconf, this,
                            boost::asio::placeholders::error));
                } else {
                    throw ipc_exception("process_task() failed to send to master: "+ec.message());
                }
            });
        break;

    case w_get_work:
        node_count = 0;

        boost::asio::async_write(socket_, boost::asio::buffer(&ipc_cnc, sizeof(ipc_cnc)),
            [this, task](boost::system::error_code ec, std::size_t)
            {
                if(!ec) {
                    dbg_1<<"sent work request to master\n";
                    boost::asio::async_read(socket_, boost::asio::buffer(&ipc_qnode, sizeof(ipc_qnode)),
                        boost::bind(&ipc_client::get_qnode, this,
                            boost::asio::placeholders::error));
                } else {
                    throw ipc_exception("process_task() failed to send to master: "+ec.message());
                }
            });
        break;

    case w_send_work:
        node_count = 0;

        //dont use lambda here due to send_to_master complexity
        boost::asio::async_write(socket_, boost::asio::buffer(&ipc_cnc, sizeof(ipc_cnc)),
            boost::bind(&ipc_client::send_qnode, this,
                boost::asio::placeholders::error));
        break;

    default:
        throw ipc_exception("task queue contains unknown or non-worker task! ("+std::to_string(task)+")\n");
    }

    //kick off ipc_service
    ipc_service.run();  //will block
    ipc_service.reset();
    dbg_1<<"process_task completed ***\n";
}

void ipc_client::get_wconf(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        dbg_1<<"got config data from master\n";
        got_config = true;
        thread_state = next_task;
    } else {
        throw ipc_exception("get_wconf() boost error: "+ec.message());
    }
}

void ipc_client::get_qnode(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        dbg_1<<"got work queue node from master ("<<node_count<<")\n";
        if(!syncing) //in case of out-of-order calling of get/send sync tasks
            syncing = true;

        get_buffer.lock.lock();
        get_buffer.data.push(ipc_qnode);
        ++get_buffer.size;
        ++node_count;
        get_buffer.lock.unlock();

        //loop until get_buffer is filled to avoid stalling crawler
        if(node_count < cfg.sc) {
            dbg_1<<"waiting for more nodes ("<<node_count<<" < "<<cfg.sc<<")\n";
            boost::asio::async_read(socket_,boost::asio::buffer(&ipc_qnode, sizeof(ipc_qnode)),
                boost::bind(&ipc_client::get_qnode, this,
                    boost::asio::placeholders::error));
        } else {
            dbg_1<<"finished getting work nodes from master\n";
            thread_state = next_task;
            syncing = false;
        }
    } else {
        throw ipc_exception("get_qnode() boost error: "+ec.message());
    }
}


//send data to master - will keep calling itself until send_buffer.size < cfg.work_presend
void ipc_client::send_qnode(const boost::system::error_code& ec) throw(std::exception)
{
    if(!ec) {
        if(!syncing) //in case of out-of-order calling of get/send sync tasks
            syncing = true;

        dbg_1<<"sent node to master\n";

        if(node_count < cfg.sc) {
            send_buffer.lock.lock();
            ipc_qnode = send_buffer.data.front();
            send_buffer.data.pop();
            --send_buffer.size;
            ++node_count;
            send_buffer.lock.unlock();

            dbg_1<<"sending "<<node_count<<" nodes to master\n";
            boost::asio::async_write(socket_, boost::asio::buffer(&ipc_qnode, sizeof(ipc_qnode)),
                boost::bind(&ipc_client::send_qnode, this,
                    boost::asio::placeholders::error));
        } else {
            dbg_1<<"drained send_buffer\n";
            thread_state = next_task;
            syncing = false;
        }
    } else {
        throw ipc_exception("send_to_master() failed to send queue_node to master: "+ec.message());
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
        throw ipc_exception("timeout: get_buffer.data empty\n");
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
    return worker_cfg;
}

//public intreface
void ipc_client::set_status(worker_status& s)
{
    status = s;
}
