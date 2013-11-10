#include <iostream>

#include "memory_mgr.hpp"
#include "cache.hpp"
#include "database.hpp"
#include "page_data.hpp"

//Local defines
#define DEBUG 5

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

memory_mgr::memory_mgr(std::string database_path, std::string user_agent)
{
    mem_cache = new cache;
    mem_db = new database(database_path);
    agent_name = user_agent;
}

memory_mgr::~memory_mgr(void)
{
    delete mem_cache;
    delete mem_db;
}

struct page_data_s* memory_mgr::get_page(std::string& url)
{
    struct page_data_s* page;

    //if cache fails to retrieve page, get it from the db
    dbg<<"trying to get page from cache\n";
    if(!mem_cache->get_page_data(&page, url)) {
        dbg<<"page not in cache, trying database\n";
        page = new struct page_data_s;
        mem_db->get_page_data(page, url);
    }

    //any page returned from memory_mgr is accessible by only one worker thread
    //database class performs simillar locking
    page->access_lock.lock();

    //if page exists in cache or db it will be filled with stored data,
    //otherwise we return a blank page.
    return page;
}

void memory_mgr::put_page(struct page_data_s* page, std::string& url)
{
    mem_db->put_page_data(page, url);
    bool ret = mem_cache->put_page_data(page, url);

    //unlock page for future access.
    page->access_lock.unlock();

    //pages that dont make it into the cache get deleted
    if(!ret) {
        dbg<<"page did not make it to cache, deleting\n";
        delete page;
    }
}

robots_txt* memory_mgr::get_robots_txt(std::string& url)
{
    robots_txt* robots;

    //if cache fails to retrieve robots_txt, get it from the db
    dbg<<"trying cache for robots_txt\n";
    if(!mem_cache->get_robots_txt(&robots, url)) {
        dbg<<"object not in cache, trying database\n";
        robots = new robots_txt(agent_name, url);
        mem_db->get_robots_txt(robots, url);
    }

    //if it exists in cache or db it will be filled with stored data,
    //otherwise we return a new robots_txt object
    return robots;
}

void memory_mgr::put_robots_txt(robots_txt* robots, std::string& url)
{
    mem_db->put_robots_txt(robots, url);
    bool ret = mem_cache->put_robots_txt(robots, url);

    if(!ret) {
        dbg<<"object did not make it to cache, deleting\n";
        delete robots;
    }
}


