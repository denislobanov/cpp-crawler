#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <ctime>
#include <unistd.h>         //sleep()
#include <glibmm/ustring.h> //utf-8 strings
#include <glibmm/convert.h> //Glib::ConvertError

#include "crawler_thread.hpp"
#include "parser.hpp"
#include "netio.hpp"
#include "robots_txt.hpp"
#include "ipc_client.hpp"
#include "page_data.hpp"
#include "ipc_common.hpp"
#include "memory_mgr.hpp"

//Local defines
#define DEBUG 4

#if defined(DEBUG)
    #define dbg std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #if DEBUG > 1
        #define dbg_1 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_1 0 && std::cout
    #endif
    #if DEBUG > 2
        #define dbg_2 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_2 0 && std::cout
    #endif
#else
    #define dbg 0 && std::cout
    #define dbg_1 0 && std::cout
    #define dbg_2 0 && std::cout
#endif

//development version, makes assumptions until IPC is in place
crawler_thread::crawler_thread(std::vector<struct tagdb_s>& parse_param)
{
    //pretend configuration
    status = READY;
    config.db_path = "./test_db";
    config.user_agent = "lcpp test";
    config.day_max_crawls = 3;
    config.parse_param = parse_param;

    netio_obj = new netio(config.user_agent);
    mem_mgr = new memory_mgr(config.db_path, config.user_agent);

    //test seed to queue
    //~ dbg<<"seed url is: "<<SEED_URL<<" initial credit "<<SEED_CREDIT<<std::endl;
    //~ struct queue_node_s preseed_node;
    //~ preseed_node.url = SEED_URL;
    //~ preseed_node.credit = SEED_CREDIT;
    //~ ipc.send_item(preseed_node);
}

crawler_thread::crawler_thread(void)
{
    //set to idle on entry to main loop
    status = ZOMBIE;
}

crawler_thread::~crawler_thread(void)
{
    delete netio_obj;
    delete mem_mgr;
}

size_t crawler_thread::root_domain(std::string& url)
{
    //consider longest scheme name
    //  01234567
    // "https://" next "/" is at the end of the root url
    dbg<<"url ["<<url<<"] root domain is char 0 -> "<<url.find_first_of("/", 8)<<std::endl;
    return url.find_first_of("/", 8);
}

void crawler_thread::crawl(queue_node_s& work_item, struct page_data_s* page, robots_txt* robots)
{
#if 0

    status = ACTIVE;
    robots->last_visit = std::time(0);

    //page rank housekeeping
    page->rank += work_item.credit;

    //parse page
    parser single_parser(work_item.url);
    single_parser.parse(config.parse_param);
    if(!single_parser.data.empty()) {
        //replaced by that returned from the crawl
        page->meta.clear();
        //process data, calculating page ranking
        unsigned int linked_pages = 0;

        try {
            for(auto& d: single_parser.data) {
                dbg_2<<"tag name ["<<d.tag_name<<"] tag data ["<<d.tag_data<<"] attr_data ["<<d.attr_data<<"]\n";

                switch(d.tag_type) {
                case url:
                    if(sanitize_url_tag(d, work_item.url)) {
                        ++linked_pages;
                        dbg_2<<"found link ["<<d.attr_data<<"]\n";
                    }
                    break;

                case meta:
                {
                    unsigned int i;
                    if((i = tokenize_meta_tag(page, d.tag_data)) > 0) {
                        dbg_2<<"found meta, "<<i<<" keywords extracted\n";
                    }
                    break;
                }

                case title:
                    if(!d.tag_data.empty()) {
                        page->title = d.tag_data;
                        //FIXME: development only
                        page->description = d.tag_data;
                        dbg_2<<"found title ["<<d.tag_data<<"]\n";
                    }
                    break;

                default:
                    //for now, do nothing
                    dbg<<"unknown tag ["<<d.tag_name<<"]\n";
                    break;
                }
            }
        } catch(Glib::ConvertError& e) {
            std::cerr<<"got a convert error  -- "<<e.what();
        }

        //FIXME: tax page here
        dbg<<"page->rank "<<page->rank<<" linked_pages "<<linked_pages<<std::endl;
        unsigned int new_credit = 0;
        if(page->rank > 0 && linked_pages > 0)
            new_credit = page->rank/linked_pages;

        page->rank = 0;
        ++page->crawl_count;
        page->last_crawl = std::chrono::system_clock::now();
        dbg<<" new_credit "<<new_credit<<std::endl;

        //put new urls on IPC work queue
        for(auto& d: single_parser.data) {
            queue_node_s new_item;

            if(d.tag_type == url) {
                new_item.url = d.attr_data;
                new_item.credit = new_credit;
                ipc.send_item(new_item);
                dbg_2<<"added ["<<new_item.url<<"] to queue\n";
            }
        }
    }
#endif
}

void crawler_thread::thread(int i) throw(std::underflow_error)
{
    while(worker_status != ZOMBIE) {
        try {
            status = IDLE;

            //get next work item from process queue
            queue_node_s work_item = ipc.get_item();
            status = ACTIVE;
            dbg<<"got work_item\n";

            //get memory
            struct page_data_s* page = mem_mgr->get_page(work_item.url);
            std::string root_url(work_item.url, 0, root_domain(work_item.url));
            dbg<<"root_url ["<<root_url<<"]\n";
            robots_txt* robots = mem_mgr->get_robots_txt(root_url);


            //robots.txt checks
            if(std::difftime(std::time(0), robots->last_visit) >= ROBOTS_REFRESH) {
                dbg<<"refreshing robots_txt\n";
                robots->fetch(*netio_obj);
            }

            //can we crawl this page?
            bool leak_all_credit = false;
            if(!robots->exclude(work_item.url)) {
                //measures to prevent excessive crawling
                std::chrono::hours one_day(24);
                if(std::chrono::duration_cast<std::chrono::hours>
                    (std::chrono::system_clock::now() - page->last_crawl) >= one_day) {

                    dbg<<"page->last_visit > 24 hours, resseting count & crawling\n";
                    page->crawl_count = 0;
                    crawl(work_item, page, robots);
                } else if(std::difftime(std::time(0), robots->last_visit) >= robots->crawl_delay) {

                    if(page->crawl_count < config.day_max_crawls) {
                        dbg<<"can crawl page\n";
                        crawl(work_item, page, robots);
                    } else {
                        leak_all_credit = true;
                    }
                } else {
                    leak_all_credit = true;
                }
            } else {
                //page is excluded, send all page credit to tax, and
                //free existing memory (it should not be stored in db)
                dbg<<"page ["<<work_item.url<<"] excluded, removing from database & memory\n";
                leak_all_credit = true;
                mem_mgr->free_page(page, work_item.url);
            }



            } else {
                dbg<<"robots_txt page excluded ["<<work_item.url<<"]\n";
                status = IDLE;

                //page credits to tax instead
            }

            //return memory
            mem_mgr->put_robots_txt(robots, root_url);
            mem_mgr->put_page(page, work_item.url);

            status = IDLE;
        } catch(std::underflow_error& e) {
            std::cerr<<"ipc work queue underrun: "<<e.what()<<std::endl;
            throw std::underflow_error("ipc_client::get_item reports empty");
        }
    }
}

void crawler_thread::main_loop(void)
{
    dbg<<"not yet implemented\n";
}

/* to do
 * use new tagdb_s configuration structur
 * sanitize should check tag/attr v enum, plus tag data etc
 * fix up missing domain, scheme etc
 *  - handle relative links
 */


bool crawler_thread::sanitize_url_tag(struct data_node_s& d, std::string root_url)
{
    bool ret = true;

    if(d.tag_name.compare("a") == 0) {
        //<a href="..."> so attr_data should always contain url
        if(!d.attr_data.empty()) {
            if(d.attr_data.substr(0, 4).compare("http") != 0) {
                dbg_1<<"trying to correct url ["<<d.attr_data<<"]\n";
                d.attr_data.insert(std::string::size_type(), root_url);
                dbg_1<<"new url ["<<d.attr_data<<"]\n";

            //FIXME: proper https support
            } else if(d.attr_data.substr(0, 5).compare("https") == 0) {
                dbg_1<<"removing ssl scheme from ["<<d.attr_data<<"]\n";
                d.attr_data.erase(4, 1);
                dbg_2<<"now ["<<d.attr_data<<"]\n";
            }
        } else {
            dbg<<"tag ["<<d.tag_name<<"] is empty, discarding\n";
            d.tag_type = invalid;
            ret = false;
        }
    } else {
        dbg<<"invalid tag name for url ["<<d.tag_name<<"]\n";
        d.tag_type = invalid;
        ret = false;
    }

    return ret;
}

bool crawler_thread::is_whitespace(Glib::ustring::value_type c)
{
    switch(c) {
    case ' ':
    case '\n':
    case '\t':
    case '\r':
    case '\f':
        return true;

    default:
        return false;
    }

    std::cerr<<"default ignored in switch!! returning false.\n";
    return false;
}

//tokenizes @data and stores each keyword as a seperate meta data entry,
//does not remove duplicates.
unsigned int crawler_thread::tokenize_meta_tag(struct page_data_s* page, Glib::ustring& data)
{
    unsigned int ret = 0;

    if(!data.empty()) {
        dbg_2<<"tokenizing meta data, original string ["<<data<<"]\n";
        Glib::ustring::size_type start = 0, end = 0;

        //we must iterate over the string chars manually as all types of
        //whitespace are valid forms of deliminators
        while(end < data.length()) {
            //manually check for whitespace as remove_if ::isspace and g_unichar_isspace fail to
            if(is_whitespace(data[end])) {
                //dont store whitespace
                if(end>start) {
                    dbg_2<<"found token ["<<data.substr(start, end-start)<<"]\n";

                    //escape string
                    page->meta.push_back(data.substr(start, end-start));
                    ++ret;
                }
                start = end+1; //dont save seperators
            }
            ++end;
        }
    }

    return ret;
}
