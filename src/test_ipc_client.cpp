#include <iostream>
#include <boost/asio.hpp>   //ipc_client()

#include "ipc_common.hpp"
#include "ipc_client.hpp"
#include "connection.hpp"

//for testing only
#include "dummy_server.hpp"

using std::cout;
using std::cerr;
using std::endl;

#define GET_SEND_LOOPS  2048

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

int main(void)
{
    cout<<">initialising test_server\n";
    dummy_server srv;
    srv.set_worker_config(worker_test_cfg);

    cout<<">initialising test_client\n";
    boost::asio::io_service io_service;
    ipc_client test_client(test_cfg, io_service);

    cout<<">pre-seeing "<<test_cfg.gbuff_min<<" queue_node_s to buffer\n";
    for(unsigned int i = 0; i< test_cfg.gbuff_min; ++i) {
        struct queue_node_s n = {.url="http://preseed_node.com/preseed", .credit = i};
        srv.push(n);
        cout<<">preseed item "<<i<<" url=["<<n.url<<"] credit=["<<n.credit<<"]\n";
    }
    cout<<">done.\n";

    cout<<">test_client getting config from server\n";
    struct worker_config ret_wcfg = test_client.get_config();

    cout<<">beggining send/get loop of "<<GET_SEND_LOOPS<<" items\n---\n";
    for(unsigned int i = 0; i < GET_SEND_LOOPS; ++i) {
        struct queue_node_s test_node = {.url = "test_url", .credit = i};
        cout<<"\n"<<i<<">SEND\n>sending test node, url=["<<test_node.url<<"] credit=["<<test_node.credit<<"]\n";
        test_client.send_item(test_node);

        //reinitialise test node = reset data
        struct queue_node_s get_node;
        cout<<"\n"<<i<<">GET\n>getting test node from client\n";
        get_node = test_client.get_item();

        cout<<">test_node url=["<<get_node.url<<"] credit=["<<get_node.credit<<"]\n";
    }
    cout<<"\n---\n>done.\n";
    return 0;
}
