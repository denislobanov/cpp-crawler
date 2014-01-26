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
#include "connection.hpp"

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

//simple test server (can only handle 1 connection from 1 client. ever.)
class test_server
{
    public:
    test_server():
        acceptor_(ipc_service, tcp::endpoint(tcp::v4(), MASTER_SERVICE_PORT)),
        connection_(ipc_service)
    {
        cout<<"server: starting test server\n";
        do_accept();
        ipc_service.run();
    }

    ~test_server()
    {
        cout<<"bye!\n";
    }

    private:
    boost::asio::io_service ipc_service;
    tcp::acceptor acceptor_;
    connection connection_;
    struct queue_node_s ipc_qnode;
    std::atomic<unsigned int> node_count;

    void do_accept(void)
    {
        cout<<"do_accept()\n";
        acceptor_.async_accept(connection_.socket(),
            [this](boost::system::error_code ec)
            {
                if(!ec) {
                    cout<<"server: accepted connection from client, waiting for initial data..\n";
                    connection_.async_read(boost::bind(&test_server::read_cnc,
                        this, boost::asio::placeholders::error));
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
            if(connection_.rdata_type() == instruction) {
                cnc_instruction ipc_cnc = connection_.rdata<cnc_instruction>();

                switch(ipc_cnc) {
                case w_register:
                    cout<<"recieved w_register from client\n";
                    connection_.wdata_type(cnc_data);
                    connection_.wdata(worker_test_cfg);
                    connection_.async_write(boost::bind(&test_server::write_complete,
                        this, boost::asio::placeholders::error));
                    break;

                case w_get_work:
                    cout<<"sending "<<test_cfg.sc<<" queue_node_s\n";

                    node_count = 1; //will send one before send_qnode handler
                    node_buffer.pop(ipc_qnode);

                    connection_.wdata_type(queue_node);
                    connection_.wdata(ipc_qnode);
                    connection_.async_write(boost::bind(&test_server::send_qnode,
                        this, boost::asio::placeholders::error));
                    break;

                case w_send_work:
                    node_count = 0;

                    cout<<"client about to send "<<test_cfg.sc<<" queue_node_s\n";
                    connection_.async_read(boost::bind(&test_server::read_qnode,
                        this, boost::asio::placeholders::error));
                    break;

                default:
                    cout<<"client sent invalid instruction "<<ipc_cnc<<endl;
                    connection_.async_read(boost::bind(&test_server::read_cnc,
                        this, boost::asio::placeholders::error));
                    break;
                }
            } else {
                cerr<<"invalid data type from client, got: "<<connection_.rdata_type()<<endl;
                connection_.async_read(boost::bind(&test_server::read_cnc,
                    this, boost::asio::placeholders::error));
            }
        } else {
            throw ipc_exception("read_cnc() boost error: "+ec.message());
        }
    }

    void write_complete(const boost::system::error_code& ec)
    {
        if(!ec) {
            cout<<"write to client successful, waiting for data\n";
            connection_.async_read(boost::bind(&test_server::read_cnc,
                this, boost::asio::placeholders::error));
        } else {
            throw ipc_exception("process_instruction::w_register::write lambda - boost error: "+ec.message());
        }
    }

    void read_qnode(const boost::system::error_code& ec)
    {
        if(!ec) {
            if(node_count < test_cfg.sc) {
                //client still writing
                if(connection_.rdata_type() == queue_node) {
                    ++node_count;
                    ipc_qnode = connection_.rdata<struct queue_node_s>();
                    node_buffer.push(ipc_qnode);

                    connection_.async_read(boost::bind(&test_server::read_qnode,
                        this, boost::asio::placeholders::error));
                } else {
                    cerr<<"client did not send a queue_node!! read "<<node_count<<" expected "<<test_cfg.sc<<endl;
                    cerr<<"data type from client "<<connection_.rdata_type()<<endl;
                    connection_.async_read(boost::bind(&test_server::read_cnc,
                        this, boost::asio::placeholders::error));
                }
            } else {
                cout<<"finished reading nodes from client\n";
                connection_.async_read(boost::bind(&test_server::read_cnc,
                    this, boost::asio::placeholders::error));
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

                connection_.wdata_type(queue_node);
                connection_.wdata(ipc_qnode);
                connection_.async_write(boost::bind(&test_server::send_qnode,
                    this, boost::asio::placeholders::error));
            } else {
                cout<<"finished sending "<<node_count<<" nodes to client\n";
                connection_.async_read(boost::bind(&test_server::read_cnc,
                    this, boost::asio::placeholders::error));
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

    sleep(2);

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
