#include <iostream>
#include <fstream>
#include <thread>   //this_thread::sleep_for
#include <chrono>   //ditto
#include <boost/asio.hpp>   //ipc_client()

#include "crawler_thread.hpp"
#include "netio.hpp"
#include "page_data.hpp"
#include "ipc_common.hpp"

//for testing only
#include "dummy_server.hpp"

using std::cout;
using std::endl;

#define INITIAL_CREDIT 100
#define CRAWL_TIME std::chrono::seconds(2*60)

//uut
static struct ipc_config ipc_cfg = {
    .gbuff_min = 2,
    .sbuff_max = 2,
    .sc = 2,
    .master_address = "127.0.0.1"
};

static struct worker_config worker_cfg = {
    .user_agent = "test_ipc_client",
    .day_max_crawls = 5,

    .page_cache_max = 10,
    .page_cache_res = 2,
    .robots_cache_max = 3,
    .robots_cache_res = 1,

    .db_path = "test_db",
    .page_table = "page_table",
    .robots_table = "robots_table"
};

int main(void)
{
    //create parser config
    struct tagdb_s param;

    param.tag_type = tag_type_url;
    param.xpath = "//a[@href]";
    param.attr = "href";
    worker_cfg.parse_param.push_back(param);

    param.tag_type = tag_type_meta;
    param.xpath = "//p";
    param.attr = "";
    worker_cfg.parse_param.push_back(param);

    param.tag_type = tag_type_title;
    param.xpath = "//title";
    param.attr = "";
    worker_cfg.parse_param.push_back(param);

    cout<<">instantiating server..\n";
    dummy_server srv;

    cout<<">configuring server\n";
    struct queue_node_s n = {.url="http://en.wikipedia.org", .credit = INITIAL_CREDIT};
    srv.push(n);
    cout<<">added seed url=["<<n.url<<"] credit=["<<INITIAL_CREDIT<<"]\n";

    srv.set_worker_config(worker_cfg);
    cout<<">done.\n";

    cout<<">creating crawler_thread\n";
    boost::asio::io_service io_service;
    ipc_client test_ipc_client(ipc_cfg, io_service);
    crawler_thread test_crawler(&test_ipc_client);

    cout<<">begin timed crawl\n";
    test_crawler.start(worker_cfg);

    //sleep for a bit, to let crawler do its thing
    std::this_thread::sleep_for(CRAWL_TIME);

    cout<<">stopping crawler\n";
    test_crawler.stop();

    //sleep for a bit, to let crawler finish its last task
    std::this_thread::sleep_for(std::chrono::seconds(1));
    cout<<"\n\n>done.\n";

    //analyse data
    return 0;
}
