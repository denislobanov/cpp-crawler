#include <iostream>
#include <sstream>
#include <chrono>   //this_thread.sleep_for()
#include <thread>
#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "ipc_common.hpp"
#include "ipc_client.hpp"

using std::cout;
using std::cerr;
using std::endl;
using boost::asio::ip::tcp;

#define GET_SEND_LOOPS  10

//internal static data
static std::atomic<bool> running;
static boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> node_buffer;
static unsigned int nodes_sent = 0;

//uut
static struct ipc_config test_cfg = {
    .get_buffer_min = 2,
    .work_presend = 2,
    .master_address = "127.0.0.1"
};

//simple test server
class test_server
{
    public:
    test_server():
        acceptor_(ipc_service, tcp::endpoint(tcp::v4(), MASTER_SERVICE_PORT)),
        socket_(ipc_service)
    {
        cout<<"server: starting test server\n";
        do_accept();
        ipc_service.run();
    }

    ~test_server()
    {
        socket_.close();
    }

    private:
    boost::asio::io_service ipc_service;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    struct ipc_message message;
    struct worker_config worker_test_cfg = {
        .user_agent = "test_ipc_client",
        .day_max_crawls = 5,

        .page_cache_max = 10,
        .page_cache_res = 2,
        .robots_cache_max = 3,
        .robots_cache_res = 1,

        .db_path = "no db"
    };

    void do_accept(void)
    {
        acceptor_.async_accept(socket_,
            [this](boost::system::error_code ec)
            {
                if(!ec) {
                    cout<<"server: accepted connection from client, waiting for initial data..\n";
                    boost::asio::async_read(socket_,
                        boost::asio::buffer(&message, sizeof(message)),
                        boost::bind(&test_server::read_handler, this,
                            boost::asio::placeholders::error));
                }
            });
    }

    void read_handler(boost::system::error_code ec)
    {
        //we're going to spend most of our time here, so this is a good
        //place to check for kill flags etc
        if(!running) {
            cout<<"server exiting\n";
            return;
        }

        if(!ec) {
            switch(message.type) {
            case instruction:
                cout<<"recieved cnc_instruction ["<<message.data.instruction<<"] size ["<<sizeof(message)<<"] from worker\n";
                process_instruction(message.data.instruction);
                break;

            case queue_node:
                cout<<"recieved queue_node from worker\n";
                node_buffer.push(message.data.node);
                boost::asio::async_read(socket_, boost::asio::buffer(&message, sizeof(message)),
                    boost::bind(&test_server::read_handler, this,
                        boost::asio::placeholders::error));
                break;

            default:
                cout<<"unknown message.type from worker ("<<message.type<<")!\n";
                throw ipc_exception("unknown message.type from worker");
                exit(-1);
            }
        } else {
            cerr<<"read_handler - boost error: "<<ec.message();
            throw ipc_exception("ead_handler - boost error");
            exit(-2);
        }
    }

    void process_instruction(cnc_instruction task)
    {
        switch(task) {
        case w_register:    //client wants config
        {
            cout<<"sending worker_config\n";
            message.type = cnc_data;
            message.data.config = worker_test_cfg;

            cout<<"message size is "<<sizeof(message)<<endl;
            boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
                [this](boost::system::error_code ec, std::size_t)
                {
                    if(!ec) {
                        cout<<"write to client successful, waiting for data\n";
                        boost::asio::async_read(socket_,
                            boost::asio::buffer(&message, sizeof(message)),
                            boost::bind(&test_server::read_handler, this,
                                boost::asio::placeholders::error));
                    } else {
                        throw ipc_exception("process_instruction::w_register::write lambda - boost error: "+ec.message());
                    }
                });
            break;
        }

        case w_get_work:    //client wants work
        {
            cout<<"sending "<<test_cfg.get_buffer_min<<" queue_node_s\n";
            ++nodes_sent;
            message.type = queue_node;
            node_buffer.pop(message.data.node);

            boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
                boost::bind(&test_server::send_nodes, this,
                    boost::asio::placeholders::error));
            break;
        }

        case w_send_work:   //client sending completed work
            cout<<"client about to send queue_node_s\n";
            boost::asio::async_read(socket_, boost::asio::buffer(&message, sizeof(message)),
                boost::bind(&test_server::read_handler, this,
                    boost::asio::placeholders::error));
            break;

        default:
            cout<<"client sent invalid instruction "<<task<<endl;
            boost::asio::async_read(socket_, boost::asio::buffer(&message, sizeof(message)),
                boost::bind(&test_server::read_handler, this,
                    boost::asio::placeholders::error));
            break;
        }
    }

    void send_nodes(const boost::system::error_code& ec)
    {
        if(!ec) {
            if(nodes_sent < test_cfg.get_buffer_min) {
                ++nodes_sent;
                message.type = queue_node;
                node_buffer.pop(message.data.node);

                boost::asio::async_write(socket_, boost::asio::buffer(&message, sizeof(message)),
                    boost::bind(&test_server::send_nodes, this,
                        boost::asio::placeholders::error));
            } else {
                cout<<"finished sending queue_node_s to worker\n";
                boost::asio::async_read(socket_, boost::asio::buffer(&message, sizeof(message)),
                    boost::bind(&test_server::read_handler, this,
                        boost::asio::placeholders::error));
            }
        } else {
            cerr<<"send_nodes - boost error: "<<ec.message();
            exit(-2);
        }
    }
};

void run_server(void)
{
    test_server server;
    cout<<"end of run_server()\n";
}

int main(void)
{
    cout<<">initialising test_server\n";
    running = true;
    std::thread srv(run_server);
    srv.detach();

    cout<<">initialising test_client\n";
    ipc_client test_client(test_cfg);

    cout<<">pre-seeing "<<test_cfg.get_buffer_min<<" queue_node_s to buffer\n";
    for(unsigned int i = 0; i< test_cfg.get_buffer_min; ++i) {
        struct queue_node_s n = {.url="http://preseed_node.com/preseed", .credit = i};
        node_buffer.push(n);
    }
    cout<<"done.\n";

    cout<<">test_client getting config from server\n";
    struct worker_config ret_wcfg = test_client.get_config();

    cout<<"\nallowing client time to buffer..\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    cout<<">beggining send/get loop of "<<GET_SEND_LOOPS<<" items\n---\n";
    for(unsigned int i = 0; i < GET_SEND_LOOPS; ++i) {
        struct queue_node_s test_node = {.url = "test_url", .credit = i};
        cout<<">sending test node, url=["<<test_node.url<<"] credit=["<<test_node.credit<<"]\n";
        test_client.send_item(test_node);

        //reinitialise test node = reset data
        test_node = {};
        cout<<">getting test node from client\n";
        test_node = test_client.get_item();

        cout<<">test_node url=["<<test_node.url<<"] credit=["<<test_node.credit<<"]\n";
    }
    cout<<"\n---\n>done.\n";
    running = false;
    return 0;
}
