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

    void set_worker_config(struct worker_config_s& worker_cfg)
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
    struct worker_config_s uut_cfg; 

    void do_accept(void)
    {
        cout<<">server: do_accept()\n";
        acceptor_.async_accept(connection_.socket(),
            [this](boost::system::error_code ec)
            {
                if(!ec) {
                    cout<<">server: accepted connection from client, waiting for initial data..\n";
                    connection_.async_read(boost::bind(&dummy_server::read_data,
                        this, boost::asio::placeholders::error));
                }
            });
        ipc_service.run();
    }

    void read_data(boost::system::error_code ec)
    {
        //we're going to spend most of our time here, so this is a good
        //place to check for kill flags etc
        if(!running) {
            cout<<">server: exiting\n";
            return;
        }

        if(!ec) {
            switch(connection_.rdata_type()) {
            case dt_instruction:
                cout<<">server: client sent ctrl_instruction"<<endl;
                process_instruction(connection_.rdata<ctrl_instruction_e>());
                break;

            case dt_wstatus:
                cout<<">server: client sent status"<<endl;
                cout<<">server: client status is "<<connection_.rdata<worker_status_e>()<<endl;
                break;

            case dt_wcap:
                cout<<">server: client sent capabilities (ignoring)"<<endl;
                //ignore
                break;

            case dt_queue_node:
                cout<<">server: cient sent queue_node_s"<<endl;
                ipc_qnode = connection_.rdata<queue_node_s>();
                node_buffer.push(ipc_qnode);
                break;

            default:
                cerr<<">server: invalid data type from client, got: "<<connection_.rdata_type()<<endl;
                break;
            }

            connection_.async_read(boost::bind(&dummy_server::read_data,
                    this, boost::asio::placeholders::error));
        } else {
            throw ipc_exception("read_cnc() boost error: "+ec.message());
        }
    }

    void process_instruction(ctrl_instruction_e instruction)
    {
        switch(instruction) {
        case ctrl_wconfig:
            cout<<">server: recieved ctrl_wconfig from client\n";

            connection_.wdata_type(dt_wconfig);
            connection_.wdata(uut_cfg);
            connection_.async_write(boost::bind(&dummy_server::write_complete,
                this, boost::asio::placeholders::error));
            break;

        case ctrl_wnodes:
            cout<<">server: recieved ctrl_wnodes from client, sending (1)queue_node_s\n";
            node_buffer.pop(ipc_qnode);

            connection_.wdata_type(dt_queue_node);
            connection_.wdata(ipc_qnode);
            connection_.async_write(boost::bind(&dummy_server::write_complete,
                this, boost::asio::placeholders::error));
            break;

        default:
            cout<<">server: recieved unknown instruction "<<instruction<<" from client"<<endl;
            break;
        }
    }

    void write_complete(const boost::system::error_code& ec)
    {
        if(!ec) {
            cout<<">server: write to client successful\n";
        } else {
            throw ipc_exception("write_complete boost error: "+ec.message());
        }
    }
};
#endif
