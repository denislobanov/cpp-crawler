#include <iostream>
#include <map>
#include <unordered_map>
#include <mutex>
#include <chrono>

#include "cache.hpp"
#include "database.hpp"
#include "robots_txt.hpp"
#include "page_data.hpp"

//Local defines
//~ #define DEBUG 2

#if defined(DEBUG)
    #define dbg std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #if DEBUG > 1
        #define dbg_1 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_1 0 && std::cout
    #endif
#else
    #define dbg 0 && std::cout
    #define dbg_1 0 && std::cout
#endif

cache::cache(void)
{
    page_ctl.fill = 0;
    robots_ctl.fill = 0;
}

cache::~cache(void)
{
    //delete all data stored in cache
    for(auto& x: page_cache)
        delete x.second.page;

    for(auto& x: robots_cache)
        delete x.second.robots;
}

void cache::update_timestamp(data_map_t& dm, access_map_t& am, std::string url, std::chrono::steady_clock::time_point new_time)
{
    //delete old timestamp from access map
    std::chrono::steady_clock::time_point old_time = dm.at(url).timestamp;
    am.erase(am.find(old_time));

    //update data timestamp
    dm.at(url).timestamp = new_time;

    //update access time
    am.insert(std::pair<std::chrono::steady_clock::time_point, std::string>(new_time, url));
}

bool cache::get_page_data(struct page_data_s** page_data, std::string& url)
{
    bool in_cache = false;

    //both getting and putting entries in cache have to be atomic
    page_ctl.rw_mutex.lock();

    try {
        *page_data = page_cache.at(url).page;
        update_timestamp(page_cache, page_access, url, std::chrono::steady_clock::now());
        in_cache = true;

        dbg<<"got page, desc: "<<(*page_data)->description<<std::endl;
    } catch(const std::out_of_range& e) {
        dbg_1<<" page ["<<url<<"] not in cache\n";
    }

    page_ctl.rw_mutex.unlock();
    return in_cache;
}

bool cache::put_page_data(struct page_data_s* page_data, std::string& url)
{
    bool in_cache = true;
    std::chrono::steady_clock::time_point new_time = std::chrono::steady_clock::now();

    page_ctl.rw_mutex.lock();
    //entries already in cache get automatically updated
    try {
        update_timestamp(page_cache, page_access, url, new_time);
        dbg<<"page ["<<url<<"] already in cache, updating\n";
    } catch(const std::out_of_range& e) {
        access_map_t::iterator pos = page_access.end();
        struct cache_entry_s entry;
        entry.page = page_data;
        entry.timestamp = new_time;


        //pages not in cache will need to be added in, if there's space
        if(page_ctl.fill < PAGE_CACHE_MAX) {
            dbg<<"space in cache to insert page ["<<url<<"]\n";
            page_cache.insert(std::pair<std::string, struct cache_entry_s>(url, entry));
            dbg_1<<"updating access map\n";
            page_access.insert(pos, std::pair<std::chrono::steady_clock::time_point,
                                              std::string>(new_time, url));
            ++page_ctl.fill;

            //kick off cache pruning job
            prune_cache(PAGE);
        } else {
            std::string oldest_url = page_access.begin()->second;

            //insert only if page.rank > oldest cached page.rank
            if(page_data->rank > page_cache.at(oldest_url).page->rank) {
                dbg<<"page ["<<url<<"] outranks oldest cache entry: "<<page_data->rank<<" v "<<page_cache.at(oldest_url).page->rank<<std::endl;
                page_access.erase(page_cache.at(oldest_url).timestamp);
                page_cache.erase(page_cache.find(oldest_url));

                page_cache.insert(std::pair<std::string, struct cache_entry_s>(url, entry));
                page_access.insert(pos, std::pair<std::chrono::steady_clock::time_point,
                                                  std::string>(new_time, url));
            } else {
                dbg<<"not putting page in cache\n";
                in_cache = false;
            }
        }
    }
    dbg<<"done\n";

    page_ctl.rw_mutex.unlock();
    return in_cache;
}

bool cache::get_robots_txt(robots_txt** robots, std::string& url)
{
    bool in_cache = false;
    //both getting and putting entries in cache have to be atomic
    robots_ctl.rw_mutex.lock();

    try {
        *robots = robots_cache.at(url).robots;
        update_timestamp(robots_cache, robots_access, url, std::chrono::steady_clock::now());
        in_cache = true;

        dbg<<"got robots\n";
    } catch(const std::out_of_range& e) {
        dbg_1<<" robots_txt for ["<<url<<"] not in cache\n";
    }

    robots_ctl.rw_mutex.unlock();
    return in_cache;
}

bool cache::put_robots_txt(robots_txt* robots, std::string& url)
{
    std::chrono::steady_clock::time_point new_time = std::chrono::steady_clock::now();

    robots_ctl.rw_mutex.lock();
    //entries already in cache get automatically updated
    try {
        update_timestamp(robots_cache, robots_access, url, new_time);
        dbg<<"robots_txt ["<<url<<"] already in cache, updating\n";
    } catch(const std::out_of_range& e) {
        //~ access_map_t::iterator pos = page_access.begin();
        struct cache_entry_s entry;
        entry.robots = robots;
        entry.timestamp = new_time;

        //pages not in cache will need to be added in, if there's space
        if(robots_ctl.fill < ROBOTS_CACHE_MAX) {
            dbg<<"space in cache to insert robots_txt ["<<url<<"]\n";
            robots_cache.insert(std::pair<std::string, struct cache_entry_s>(url, entry));
            dbg_1<<"updating access map\n";
            robots_access.insert(std::pair<std::chrono::steady_clock::time_point,
                                           std::string>(new_time, url));
            ++robots_ctl.fill;

            //kick off cache pruning job
            prune_cache(ROBOTS);
        } else {
            //kick oldest item if stale
            std::string oldest_url = robots_access.begin()->second;
            dbg<<"kicking old robots_txt ["<<oldest_url<<"] out of cache for ["<<url<<"]\n";

            robots_access.erase(robots_cache.at(oldest_url).timestamp);
            robots_cache.erase(robots_cache.find(oldest_url));
            robots_cache.insert(std::pair<std::string, struct cache_entry_s>(url, entry));
            dbg_1<<"updating access map\n";
            robots_access.insert(std::pair<std::chrono::steady_clock::time_point,
                                 std::string>(new_time, url));
        }
    }

    robots_ctl.rw_mutex.unlock();
    return true;
}

void cache::prune_cache(cache_type t)
{
    data_map_t* dm;
    access_map_t* am;
    cache_ctl_s* ctl;
    int max_fill, reserve;

    if(t == PAGE) {
        dbg<<"pruning page cache\n";
        dm = &page_cache;
        am = &page_access;
        ctl = &page_ctl;
        max_fill = PAGE_CACHE_MAX;
        reserve = PAGE_CACHE_RES;
    } else {
        dbg<<"pruning robots cache\n";
        dm = &robots_cache;
        am = &robots_access;
        ctl = &robots_ctl;
        max_fill = ROBOTS_CACHE_MAX;
        reserve = ROBOTS_CACHE_RES;
    }

    //FIXME: put this into a thread
    dbg_1<<"ctl->fill "<<ctl->fill<<std::endl;
    dbg_1<<"max_fill "<<max_fill<<std::endl;
    dbg_1<<"reserve "<<reserve<<std::endl;

    while(ctl->fill-(max_fill-reserve) > 0) {
        dbg_1<<"pages to prune "<<ctl->fill-(max_fill-reserve)<<std::endl;

        //free memory
        delete dm->at(am->begin()->second).page;
        delete dm->at(am->begin()->second).robots;

        dm->erase(dm->find(am->begin()->second));
        am->erase(am->begin());
        --ctl->fill;
        dbg_1<<"fill "<<ctl->fill<<std::endl;
        exit(-5);
    }
}

void cache::rm_page_data(struct page_data_s* page_data, std::string& url)
{
    dbg<<"FIXME: cache::rm_page_data not implemented\n";
}
