#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>

#include "ipc_client.hpp"

using std::cout;
using std::endl;
using boost::asio::ip::tcp;

//thread ctrl
static std::atomic<bool> running = false;
static std::atomic<bool> kill_threads = false;

//global data (for async handlers)
static unsigned int sent_data = 0;
static unsigned int got_data = 0;
static struct ipc_message data;
static boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> node_buffer;
static struct worker_config worker_test_cfg = {
    .user_agent = "test_ipc_client";
    .day_max_crawls = 5;
    
    .page_cache_max = 10;
    .page_cache_res = 2;
    .robots_cache_max = 3;
    .robots_cache_res = 1;

    .db_path = "no db";
    .parse_param = {0};
};
static tcp::socket socket;


//not sure on these two
void send_thread(ipc_client& uut)
{
    unsigned int test_credit = 0;

    while(!kill_threads) {
        if(running) {
            //generate test url
            std::stringstream ss;
            ss << "http://test_url.com/test_page_"<<test_credit<<".html";

            //generate test node
            struct queue_node_s test_node = {
                .credit = test_credit;
                .url = ss.str();
            };

            //send test data to uut
            uut.send_item(test_node);
            cout<<"-->sent to uut url["<<test_node.url<<"] credit ["<<test_node.credit<<"]\n";
            ++sent_data;
        }

        std::this_thread::yield();
    }
}

void get_thread(ipc_client& uut)
{
    while(!kill_threads) {
        if(running) {
            //drain uut
            struct queue_node_s test_node = uut.get_item();
            cout<<"<--got from uut url["<<test_node.url<<"] credit ["<<test_node.credit<<"]\n";
            ++got_data;
        }

        std::this_thread::yield();
    }
}

void write_finished(const boost::system::error_code& err) throw(std::system_error)
{

}

void read_from_uut(const boost::system::error_code& err) throw(std::system_error)
{
    if(!err) {
        switch(data.type) {
        case instruction:
        {
            switch(data.data.instruction) {
            case w_register:
            {
                //send config
                data = {
                    .type = cnc_data;
                    .data.config = worker_test_cfg;
                };
                boost::asio::async_write(socket, data,
                    boost::bind(write_finished, this, boost::asio::placeholders::error));
                break;
            }
            case w_get_work:
            {
                //send from node_buffer
                data.type = queue_node;
                if(!node_buffer.pop(data.data.queue_node))
                    throw std::system_error("get_buff buffer queue empty\n");
                
                boost::asio::async_write(socket, data,
                    boost::bind(write_finished, this, boost::asio::placeholders::error));
                break;

            case w_send_work:
                //read to node_buffer
                break;

            default:
                cerr<<"unknown instruction ["<<data.data.instruction<<"] (got data type instruction["<<data.type<<"])\n";
                break;
            }
            
            break;
        }

        case queue_node:
            cout<<"worker sent queue node\n";
            //read to node_buffer
            break;

        default:
            cerr<<"unknown data type ["<<data.type<<"]\n";
            throw std::system_error("unknown data from master: "<<err.message()<<endl);
            break;
        }
    } else {
        cerr<<"boost::asio error: "<<err.message()<<endl;
    }
}

int main(void)
{
    //local ctrl
    const unsigned int loops = 10;
    std::thread st, gt;
    boost::asio::io_service io_service;
    struct ipc_config test_cfg = {
        .work_prebuff = 2;
        .get_buffer_min = 1;
        .work_presend = 2;
        .master_address = "127.0.0.1";
    };

    //instantiate psuedo master    
    try {
        boost::asio::io_service io_service;
        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), MASTER_SERVICE_PORT));

        socket(io_service);
        acceptor.async_accept(socket,
            [this](boost::system::error_code ec)
            {
                if(!ec) {
                    //uut has connected. work in response mode only
                    socket.async_read(socket, data,
                        boost::asio::transfer_all(sizeof(data)),
                        boost::bind(read_from_uut, this,
                            boost::asio::placeholders::error));
                }
            }
    } catch(exception& e) {
        cerr<<e.what();
        exit(-1);
    }

    //instantiate client
    ipc_client uut(test_cfg);

    //threads sleep until running == true
    st = std::thread(send_thread);
    st.detatch();
    gt = std::thread(get_thread);
    gt.detatch();

    cout<<"testing configuration retrieval.. ";
    struct worker_config ret_wcfg = uut.get_config();

    if(ret_wcfg == worker_test_cfg) {
        cout<<"config matches\n";
        running = true;
    } else {
        //end test
        cout<<"config mismatch FAIL\n";
        kill_threads = true;
    }

    if(running) {
        
    
    

    

    

    return 0;
}
