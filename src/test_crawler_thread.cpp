#include <iostream>
#include <fstream>

#include "crawler_thread.hpp"
#include "netio.hpp"
#include "page_data.hpp"
#include "ipc_common.hpp"

using std::cout;
using std::endl;

#define CRAWL_LOOPS 10000

int main(void)
{
    //create parser config
    std::vector<struct tagdb_s> parse_param;
    struct tagdb_s param;

    param.tag_type = url;
    param.xpath = "//a[@href]";
    param.attr = "href";
    parse_param.push_back(param);

    param.tag_type = meta;
    param.xpath = "//p";
    param.attr = "";
    parse_param.push_back(param);

    param.tag_type = title;
    param.xpath = "//title";
    param.attr = "";
    parse_param.push_back(param);

    cout<<"creating crawler_thread"<<endl;
    crawler_thread test_crawler(parse_param);

    cout<<"begin crawl of "<<CRAWL_LOOPS<<" pages\n";
    test_crawler.dev_loop(CRAWL_LOOPS);

    //analyse data
#if 0
    cout<<"test queue contents are\n";
    int i = 0;
    while(true) {
        struct queue_node_s node = test_queue.fetch();
        cout<<"node "<<i<<" url ["<<node.url<<"] credit "<<node.credit<<endl;
    }
#endif

    return 0;
}
