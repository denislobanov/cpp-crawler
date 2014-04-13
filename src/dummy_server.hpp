#if !defined (DUMMY_SERVER_H)
#define DUMMY_SERVER_H

#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include "ipc_client.hpp"
#include "connection.hpp"

using std::cout;
using std::cerr;
using std::endl;
using boost::asio::ip::tcp;

//simple test server (can only handle 1 connection from 1 client. ever.)
class dummy_server
{
    public:
    dummy_server():
        acceptor_(ipc_service, tcp::endpoint(tcp::v4(), MASTER_SERVICE_PORT)),
        connection_(ipc_service)
    {
        cout<<">server: starting test server\n";
        running = true;

        srv = std::thread(&dummy_server::do_accept, this);
    }

    ~dummy_server()
    {
        running = false;
        cout<<">server: bye!\n";
    }

    void push(struct queue_node_s& n)
    {
        node_buffer.push(n);
    }

    void set_worker_config(struct worker_config& worker_cfg)
    {
        uut_cfg = worker_cfg;
    }

    private:
    //ipc io
    boost::asio::io_service ipc_service;
    tcp::acceptor acceptor_;
    connection connection_;
    struct queue_node_s ipc_qnode;

    //server thread
    std::thread srv;
    std::atomic<bool> running;
    boost::lockfree::spsc_queue<struct queue_node_s, boost::lockfree::capacity<BUFFER_MAX_SIZE>> node_buffer;

    //thread data
    struct worker_config uut_cfg; 

    void do_accept(void)
    {
        cout<<">do_accept()\n";
        acceptor_.async_accept(connection_.socket(),
            [this](boost::system::error_code ec)
            {
                if(!ec) {
                    cout<<">server: accepted connection from client, waiting for initial data..\n";
                    connection_.async_read(boost::bind(&dummy_server::read_cnc,
                        this, boost::asio::placeholders::error));
                }
            });
        ipc_service.run();
    }

    void read_cnc(boost::system::error_code ec)
    {
        //we're going to spend most of our time here, so this is a good
        //place to check for kill flags etc
        if(!running) {
            cout<<">server: exiting\n";
            return;
        }

        if(!ec) {
            if(connection_.rdata_type() == instruction) {
                cnc_instruction ipc_cnc = connection_.rdata<cnc_instruction>();

                switch(ipc_cnc) {
                case w_register:
                    cout<<">server: recieved w_register from client\n";
                    connection_.wdata_type(cnc_data);
                    connection_.wdata(uut_cfg);
                    connection_.async_write(boost::bind(&dummy_server::write_complete,
                        this, boost::asio::placeholders::error));
                    break;

                case w_get_work:
                    cout<<">server: sending queue_node_s to client\n";
                    node_buffer.pop(ipc_qnode);

                    connection_.wdata_type(queue_node);
                    connection_.wdata(ipc_qnode);
                    connection_.async_write(boost::bind(&dummy_server::read_cnc,
                        this, boost::asio::placeholders::error));
                    break;

                case w_send_work:
                    cout<<">server: client about to send a queue_node_s\n";
                    connection_.async_read(boost::bind(&dummy_server::read_qnode,
                        this, boost::asio::placeholders::error));
                    break;

                default:
                    cout<<">server: client sent invalid instruction "<<ipc_cnc<<endl;
                    connection_.async_read(boost::bind(&dummy_server::read_cnc,
                        this, boost::asio::placeholders::error));
                    break;
                }
            } else {
                cerr<<">server: invalid data type from client, got: "<<connection_.rdata_type()<<endl;
                connection_.async_read(boost::bind(&dummy_server::read_cnc,
                    this, boost::asio::placeholders::error));
            }
        } else {
            throw ipc_exception("read_cnc() boost error: "+ec.message());
        }
    }

    void write_complete(const boost::system::error_code& ec)
    {
        if(!ec) {
            cout<<">server: write to client successful, waiting for data\n";
            connection_.async_read(boost::bind(&dummy_server::read_cnc,
                this, boost::asio::placeholders::error));
        } else {
            throw ipc_exception("process_instruction::w_register::write lambda - boost error: "+ec.message());
        }
    }

    void read_qnode(const boost::system::error_code& ec)
    {
        if(!ec) {
            if(connection_.rdata_type() == queue_node) {
                ipc_qnode = connection_.rdata<struct queue_node_s>();
                node_buffer.push(ipc_qnode);

            } else {
                cerr<<">server: client did not send a queue_node!\n";
                cerr<<">server: data type from client "<<connection_.rdata_type()<<endl;
            }

            connection_.async_read(boost::bind(&dummy_server::read_cnc,
                this, boost::asio::placeholders::error));

        } else {
            throw ipc_exception("read_qnode() boost error: "+ec.message());
        }
    }
};
#endif
