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
    connection_(ipc_service), resolver_(ipc_service)
{
    cfg = config;

    //initialise internal data
    thread_state = st_stop;
    syncing = false;
    got_config = false;
    
    wstatus = IDLE;
    wcfg = {};

    //launch background service thread
    task_thread = std::thread(&ipc_client::ipc_thread, this);
    //~ task_thread.detach();
    dbg_1<<"task_thread detatched\n";
}

ipc_client::~ipc_client(void)
{
    //tell thread to shut down
    syncing = true;
    thread_state = st_stop;
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
                boost::asio::async_connect(connection_.socket(), it,
                    boost::bind(&ipc_client::handle_connected, this,
                        boost::asio::placeholders::error));
            } else {
                throw ipc_exception("async_resolve error: "+ec.message());
            }
        });

    dbg<<"launching boost service\n";
    thread_state = st_run;
    ipc_service.run();
    ipc_service.reset();

    while(thread_state >= st_run) {
        if(thread_state >= st_connected) {
            //prevents spamming of master and makes sure follow up replies/reads occure in order
            if(thread_state == st_next_task) {
                if(task_queue.size()) {
                    cnc_instruction task = task_queue.pop();

                    dbg<<"processing task: "<<task<<" ***\n";
                    process_task(task);
                }
            }

            if(!syncing) {
                //dont fill get_buffer until we have a connection (long connection
                //times can result in > cfg.get_buffer_min w_get_work tasks on queue)
                if(get_buffer.size() < cfg.gbuff_min) {
                    syncing = true;
                    dbg_1<<"adding w_get_work task to queue\n";
                    task_queue.push(w_get_work);
                }

                //send_to_master drains queue down to work_presend
                if(send_buffer.size() > cfg.sbuff_max) {
                    syncing = true;
                    dbg_1<<"adding w_send_work task to queue\n";
                    task_queue.push(w_send_work);
                }

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
        thread_state = st_next_task;
        dbg<<"thread_state: "<<thread_state<<std::endl;
        dbg<<"connected.\n";
    } else {
        throw ipc_exception("async_connect error in handle except: "+ec.message());
    }
}

//there must not be any outstanding reads when this function enters
void ipc_client::process_task(cnc_instruction task) throw(std::exception)
{
    thread_state = st_processing;
    connection_.wdata_type(instruction);
    connection_.wdata(task);

    switch(task) {
    case w_register:
        connection_.async_write(boost::bind(&ipc_client::write_complete, this,
            boost::asio::placeholders::error));
        break;

    case w_get_work:
        nodes_io = 0;
        connection_.async_write(boost::bind(&ipc_client::write_complete, this,
            boost::asio::placeholders::error));
        break;

    case w_send_work:
        nodes_io = 0;
        //dont use lambda here due to send_to_master complexity
        connection_.async_write(boost::bind(&ipc_client::send_qnode, this,
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
            cnc_instruction i = connection_.rdata<cnc_instruction>();
            task_queue.push(i);
            thread_state = st_next_task;
            break;
        }
        
        case cnc_data:
        {
            wcfg = connection_.rdata<worker_config>();
            thread_state = st_next_task;
            got_config = true;
            break;
        }
        
        case queue_node:
        {
            queue_node_s n = connection_.rdata<queue_node_s>();
            get_buffer.push(n);
            ++nodes_io;

            if(nodes_io >= cfg.sc) {
                thread_state = st_next_task;
                syncing = false;
            } else {
                dbg_1<<"asking for more nodes (node_io is "<<nodes_io<<")\n";
                connection_.async_read(boost::bind(&ipc_client::read_data, this,
                    boost::asio::placeholders::error));
            }
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
        if(!syncing) //in case of out-of-order calling of get/send sync tasks
            syncing = true;

        dbg_1<<"sent node to master\n";

        if(nodes_io < cfg.sc) {
            connection_.wdata_type(queue_node);
            connection_.wdata(send_buffer.pop());

            dbg_1<<"sending "<<nodes_io<<" nodes to master\n";
            connection_.async_write(boost::bind(&ipc_client::send_qnode, this,
                boost::asio::placeholders::error));
        } else {
            dbg_1<<"drained send_buffer\n";
            thread_state = st_next_task;
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
    send_buffer.push(data);
    dbg<<"added work item to queue, size is now: "<<send_buffer.size()<<std::endl;
}

//public interface
//get item from get_buffer, will block if buffer is empty until it has been
//filled by task_thread. Will throw exception if blocked for t seconds
struct queue_node_s ipc_client::get_item(unsigned int t) throw(std::exception)
{
    unsigned int dt = 0;
    struct queue_node_s data = {};

    //timed block if buffer is empty
    while(get_buffer.size() == 0 && dt++ < t)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    if(get_buffer.size() > 0) {
        dbg<<"data present on queue\n";
        data = get_buffer.pop();
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

    if(get_buffer.size() > 0) {
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
    task_queue.push(w_register);
    dbg<<"added w_register task to queue\n";

    //block whilst waiting for master to send config
    while(!got_config)
        std::this_thread::sleep_for(SERVICE_GRANUALITY);

    //reset for next run
    got_config = false;
    return wcfg;
}

//public intreface
void ipc_client::set_status(worker_status& s)
{
    wstatus = s;
}
