#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <ctime>
#include <glibmm/ustring.h> //utf-8 strings
#include <glibmm/convert.h> //Glib::ConvertError
#include <thread>

#include "crawler_thread.hpp"
#include "parser.hpp"
#include "netio.hpp"
#include "robots_txt.hpp"
#include "ipc_client.hpp"
#include "page_data.hpp"
#include "ipc_common.hpp"
#include "memory_mgr.hpp"
#include "debug.hpp"

//
//Local defines
#define CREDIT_TAX_PERCENT 10
#define CREDIT_TAX_ALL 100

//Time in *microseconds* between aborting crawls due to domain crawl_delay
//before thread should start to implement sleep time. This stops thread from
//spinning in situations where the work queue is dominatied by one (or few)
//domains.
#define TOO_MANY_RETRIES_TIME 1000

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;

//
//public
crawler_thread::crawler_thread(ipc_client* ipc_obj)
{
    //set to idle on entry to main loop
    thread_status = SLEEP;
    ipc = ipc_obj;
}

crawler_thread::~crawler_thread(void)
{
    delete netio_obj;
}

//try and get config via ipc_client
void crawler_thread::start(void)
{
    cfg = ipc->get_config();
    netio_obj = new netio(cfg.user_agent);

    launch_thread();
}

//pre-defined config data
void crawler_thread::start(worker_config_s& config)
{
    cfg = config;
    netio_obj = new netio(cfg.user_agent);

    launch_thread();
}

void crawler_thread::stop(void)
{
    thread_status = STOP;
}

worker_status_e crawler_thread::status(void)
{
    return thread_status;
}

//
//private
void crawler_thread::launch_thread(void)
{
    main_thread = std::thread(&crawler_thread::thread, this);
    dbg<<"launched thread\n";

    //main_thread.detatch()
}

void crawler_thread::thread() throw(std::underflow_error)
{
    mmgr_config page_mgr_cfg = {
        .database_path = cfg.db_path,
        .object_table = cfg.page_table,
        .user_agent = cfg.user_agent
    };
    mmgr_config robots_mgr_cfg = {
        .database_path = cfg.db_path,
        .object_table = cfg.robots_table,
        .user_agent = cfg.user_agent
    };
    memory_mgr<page_data_c> page_mgr(page_mgr_cfg);
    memory_mgr<robots_txt> robots_mgr(robots_mgr_cfg);
    std::chrono::system_clock::time_point last_backoff = std::chrono::system_clock::now();
    microseconds sleep_time(TOO_MANY_RETRIES_TIME);

    while(thread_status > STOP) {
        try {
            thread_status = IDLE;

            //get next work item from process queue
            queue_node_s work_item = ipc->get_item();
            thread_status = ACTIVE;
            dbg<<"got work_item\n";

            //get memory
            page_data_c* page = page_mgr.get_object_nblk(work_item.url);
            std::string root_url(work_item.url, 0, root_domain(work_item.url));
            std::cout<<"root_url ["<<root_url<<"]\n";

            robots_txt* robots = robots_mgr.get_object_nblk(root_url);
            robots->configure(cfg.user_agent, root_url);

            //robots.txt checks
            seconds robots_refresh_time(ROBOTS_REFRESH);
            std::chrono::system_clock::time_point now_time = std::chrono::system_clock::now();

            if(duration_cast<seconds> (now_time - robots->last_visit())
               >= robots_refresh_time) {
                dbg<<"refreshing robots_txt\n";
                robots->fetch(*netio_obj);
                //robots last_visit time is updated automatically
            }

            //can we crawl this page?
            if(!robots->exclude(work_item.url)) {
                //measures to prevent excessive crawling
                std::chrono::hours one_day(24);

                //domain crawl timeout is enforced via the (domain) root page
                page_data_c* root_page;
                if(work_item.url != root_url)
                    root_page = page_mgr.get_object_nblk(root_url);
                else
                    root_page = page;

                //if a page from the root domain has been recently crawled or,
                // *this* page has been crawled too often already (past 24hrs)
                //then, we requeue the work order and get on with something
                //else to avoid stalling.
                if((duration_cast<seconds>(now_time - root_page->last_crawl)
                   >= robots->crawl_delay()) and (page->crawl_count < cfg.day_max_crawls)) {

                    //special case
                    if(duration_cast<std::chrono::hours> (now_time - page->last_crawl)
                       >= one_day) {
                        dbg<<"page->last_visit > 24 hours, resetting count & crawling\n";
                        page->crawl_count = 0;
                    }

                    dbg<<"crawling page ["<<work_item.url<<"]\n";
                    crawl(work_item, page, robots);
                    //reset to default
                    sleep_time = microseconds(TOO_MANY_RETRIES_TIME);

                } else {
                    dbg<<"page ["<<work_item.url<<"] has either been recently crawled or root domain ["<<root_url<<"] has recently been visited\n";
                    dbg_2<<"crawl count: "<<page->crawl_count<<std::endl;
                    dbg_2<<"root domain last crawl time: "<<std::chrono::system_clock::to_time_t(root_page->last_crawl)<<std::endl;
                    dbg_2<<"delta to now: "<<duration_cast<seconds>(now_time - root_page->last_crawl).count()<<std::endl;
                    dbg_2<<"crawl delay: "<<robots->crawl_delay().count()<<std::endl;

                    //heuristic back-off
                    if((duration_cast<seconds>(now_time - root_page->last_crawl)
                       < robots->crawl_delay()) and (duration_cast<microseconds>
                       (now_time - last_backoff) >= microseconds(TOO_MANY_RETRIES_TIME)))
                    {
                        sleep_time += duration_cast<microseconds>(seconds(1));

                        if(duration_cast<seconds>(sleep_time) > robots_refresh_time) {
                            sleep_time = duration_cast<microseconds>(robots_refresh_time);
                        }
                        dbg<<"upping backoff, sleep_time now "<<sleep_time.count()<<std::endl;
                    }
                    last_backoff = std::chrono::system_clock::now();

                    //re-queue page for later processing
                    ipc->send_item(work_item);
                }

                page_mgr.put_object_nblk(page, work_item.url);
                if(work_item.url != root_url)
                    page_mgr.put_object_nblk(root_page, root_url);

            //domains robots.txt lists this page as now excluded, so we
            //remove it from the database.
            } else {
                dbg<<"page ["<<work_item.url<<"] excluded, removing from database & memory\n";

                //send all page credit to tax
                page->rank = tax(work_item.credit + page->rank, CREDIT_TAX_ALL);
                page_mgr.delete_object_nblk(page, work_item.url);
            }

            //robots_txt no longer needed
            robots_mgr.put_object_nblk(robots, root_url);

            dbg<<">done.\n";
            thread_status = IDLE;
            std::this_thread::sleep_for(sleep_time);
        } catch(std::underflow_error& e) {
            thread_status = ZOMBIE;
            std::cerr<<"ipc work queue underrun: "<<e.what()<<std::endl;
            throw std::underflow_error("ipc_client::get_item reports empty");
        }
    }
}

void crawler_thread::crawl(queue_node_s& work_item, page_data_c* page, robots_txt* robots)
{
    //parse page
    parser page_parser(work_item.url);
    page_parser.parse(cfg.parse_param);

    if(!page_parser.data.empty()) {
        //will be replaced by new data from parser
        page->meta.clear();

        //process data, calculating page ranking
        unsigned int linked_pages = 0;
        try {
            for(auto& d: page_parser.data) {
                dbg_2<<"tag name ["<<d.tag_name<<"] tag data ["<<d.tag_data<<"] attr_data ["<<d.attr_data<<"]\n";

                switch(d.tag_type) {
                case tag_type_url:
                    if(sanitize_url_tag(d, work_item.url)) {
                        ++linked_pages;
                        dbg_2<<"found link ["<<d.attr_data<<"]\n";
                    }
                    break;

                case tag_type_meta:
                {
                    unsigned int i;
                    if((i = tokenize_meta_tag(page, d.tag_data)) > 0) {
                        dbg_2<<"found meta, "<<i<<" keywords extracted\n";
                    }
                    break;
                }

                case tag_type_title:
                    if(!d.tag_data.empty()) {
                        page->title = d.tag_data;
                        dbg_2<<"found title ["<<d.tag_data<<"]\n";
                    }
                    break;

                case tag_type_description:
                    if(!d.tag_data.empty()) {
                        page->description += d.tag_data;
                        dbg_2<<"found page description str, adding ["<<d.tag_data<<"]\n";
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

        //add referrer credit
        page->rank += work_item.credit;
        dbg<<"page->rank "<<page->rank<<" linked_pages "<<linked_pages<<std::endl;
        page->rank = tax(page->rank, CREDIT_TAX_PERCENT);
        dbg<<"page->rank after tax: "<<page->rank<<std::endl;

        unsigned int transfer_credit = 0;
        if(page->rank > 0 && linked_pages > 0)
            transfer_credit = page->rank/linked_pages;
        page->rank = 0;

        ++page->crawl_count;
        page->last_crawl = std::chrono::system_clock::now();
        dbg<<"page->crawl_count "<<page->crawl_count<<" transfer_credit "<<transfer_credit<<std::endl;

        //new URLs used to generate work_items
        for(auto& d: page_parser.data) {
            queue_node_s new_item;

            if(d.tag_type == tag_type_url) {
                new_item.url = d.attr_data;
                new_item.credit = transfer_credit;
                ipc->send_item(new_item);
                dbg_2<<"added ["<<new_item.url<<"] to queue\n";
            }
        }
    }
}

unsigned int crawler_thread::tax(unsigned int credit, unsigned int percent)
{
    unsigned int leak = credit*(percent/100);
    dbg<<"credit pre tax: "<<credit<<" post tax: "<<credit-leak<<" (taxed: "<<leak<<" @ "<<percent<<"%)\n";

    return credit-leak;
}

size_t crawler_thread::root_domain(std::string& url)
{
    //consider longest scheme name
    //  01234567
    // "https://" next charecter '/','#' or '?' is at the end of the root url
    size_t ret = url.find_first_of("/#?", 8);
    if(ret > url.length())
        ret = url.length();

    dbg<<"url ["<<url<<"] root domain is char 0 -> "<<ret<<std::endl;
    return ret;
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
            d.tag_type = tag_type_invalid;
            ret = false;
        }
    } else {
        dbg<<"invalid tag name for url ["<<d.tag_name<<"]\n";
        d.tag_type = tag_type_invalid;
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
unsigned int crawler_thread::tokenize_meta_tag(page_data_c* page, Glib::ustring& data)
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
