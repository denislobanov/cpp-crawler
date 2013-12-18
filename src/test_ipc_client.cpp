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

//uut
static struct ipc_config test_cfg = {
    .gbuff_min = 2,
    .sbuff_max = 2,
    .sc = 2,
    .master_address = "127.0.0.1"
};

static struct worker_config worker_test_cfg = {
    .user_agent = "test_ipc_client",
    .day_max_crawls = 5,

    .page_cache_max = 10,
    .page_cache_res = 2,
    .robots_cache_max = 3,
    .robots_cache_res = 1,

    .db_path = "no db"
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
    cnc_instruction ipc_cnc;
    struct queue_node_s ipc_qnode;
    std::atomic<unsigned int> node_count;

    void do_accept(void)
    {
        acceptor_.async_accept(socket_,
            [this](boost::system::error_code ec)
            {
                if(!ec) {
                    cout<<"server: accepted connection from client, waiting for initial data..\n";
                    boost::asio::async_read(socket_,
                        boost::asio::buffer(&ipc_cnc, sizeof(ipc_cnc)),
                        boost::bind(&test_server::read_cnc, this,
                            boost::asio::placeholders::error));
                }
            });
    }

    void read_cnc(boost::system::error_code ec)
    {
        //we're going to spend most of our time here, so this is a good
        //place to check for kill flags etc
        if(!running) {
            cout<<"server exiting\n";
            return;
        }

        if(!ec) {
            switch(ipc_cnc) {
            case w_register:
                cout<<"recieved w_register from client\n";
                boost::asio::async_write(socket_, boost::asio::buffer(&worker_test_cfg, sizeof(worker_test_cfg)),
                [this](boost::system::error_code ec, std::size_t)
                {
                    if(!ec) {
                        cout<<"write to client successful, waiting for data\n";
                        boost::asio::async_read(socket_,
                            boost::asio::buffer(&ipc_cnc, sizeof(ipc_cnc)),
                            boost::bind(&test_server::read_cnc, this,
                                boost::asio::placeholders::error));
                    } else {
                        throw ipc_exception("process_instruction::w_register::write lambda - boost error: "+ec.message());
                    }
                });
                break;

            case w_get_work:
                cout<<"sending "<<test_cfg.sc<<" queue_node_s\n";

                node_count = 1; //will send one before send_qnode handler
                node_buffer.pop(ipc_qnode);

                boost::asio::async_write(socket_, boost::asio::buffer(&ipc_qnode, sizeof(ipc_qnode)),
                    boost::bind(&test_server::send_qnode, this,
                        boost::asio::placeholders::error));
                break;

            case w_send_work:
                node_count = 0;

                cout<<"client about to send "<<test_cfg.sc<<" queue_node_s\n";
                boost::asio::async_read(socket_, boost::asio::buffer(&ipc_qnode, sizeof(ipc_qnode)),
                    boost::bind(&test_server::read_qnode, this,
                        boost::asio::placeholders::error));
                break;

            default:
                cout<<"client sent invalid instruction "<<ipc_cnc<<endl;
                boost::asio::async_read(socket_, boost::asio::buffer(&ipc_cnc, sizeof(ipc_cnc)),
                    boost::bind(&test_server::read_cnc, this,
                        boost::asio::placeholders::error));
                break;
            }
        } else {
            throw ipc_exception("read_cnc() boost error: "+ec.message());
        }
    }

    void read_qnode(const boost::system::error_code& ec)
    {
        if(!ec) {
            if(node_count < test_cfg.sc) {
                ++node_count;
                node_buffer.push(ipc_qnode);

                boost::asio::async_read(socket_, boost::asio::buffer(&ipc_qnode, sizeof(ipc_qnode)),
                    boost::bind(&test_server::read_qnode, this,
                        boost::asio::placeholders::error));
            } else {
                cout<<"finished reading nodes from client\n";
                boost::asio::async_read(socket_, boost::asio::buffer(&ipc_cnc, sizeof(ipc_cnc)),
                boost::bind(&test_server::read_cnc, this,
                    boost::asio::placeholders::error));
            }
        } else {
            throw ipc_exception("read_qnode() boost error: "+ec.message());
        }
    }

    void send_qnode(const boost::system::error_code& ec)
    {
        if(!ec) {
            if(node_count < test_cfg.sc) {
                ++node_count;
                node_buffer.pop(ipc_qnode);

                boost::asio::async_write(socket_, boost::asio::buffer(&ipc_qnode, sizeof(ipc_qnode)),
                    boost::bind(&test_server::send_qnode, this,
                        boost::asio::placeholders::error));
            } else {
                cout<<"finished sending "<<node_count<<" nodes to client\n";
                boost::asio::async_read(socket_, boost::asio::buffer(&ipc_cnc, sizeof(ipc_cnc)),
                    boost::bind(&test_server::read_cnc, this,
                        boost::asio::placeholders::error));
            }
        } else {
            throw ipc_exception("send_qnode() boost error: "+ec.message());
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

    cout<<">pre-seeing "<<test_cfg.gbuff_min*2<<" queue_node_s to buffer\n";
    for(unsigned int i = 0; i< test_cfg.gbuff_min*2; ++i) {
        struct queue_node_s n = {.url="http://preseed_node.com/preseed", .credit = i};
        node_buffer.push(n);
        cout<<">preseed item "<<i<<" url=["<<n.url<<"] credit=["<<n.credit<<"]\n";
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
        struct queue_node_s get_node;
        cout<<">getting test node from client\n";
        get_node = test_client.get_item(4);

        cout<<">test_node url=["<<get_node.url<<"] credit=["<<get_node.credit<<"]\n";
    }
    cout<<"\n---\n>done.\n";
    running = false;
    return 0;
}
