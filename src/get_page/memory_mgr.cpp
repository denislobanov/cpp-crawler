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
    struct page_data_s* page = new struct page_data_s;
    page->robots = new robots_txt(agent_name, url);

    //see if page exists in cache
    //cache will autimatically lock the page access mutex,
    //thus any page returned from cache can only be handled by one thread.
    if(mem_cache->get_page_data(&page, url))
        return page;

    //try db instead
    mem_db->get_page_data(page, url);

    //will need to lock access now, thus any page returned from memory_mgr
    //is accessible by only one thread
    page->access_lock.lock();

    //if page exists in db, it will be filled with stored data, otherwise
    //we return a blank page.
    return page;
}

void memory_mgr::put_page(std::string& url, struct page_data_s* page)
{
    mem_db->put_page_data(page, url);
    bool ret = mem_cache->put_page_data(page, url);

    //unlock page for future access.
    page->access_lock.unlock();

    //pages that dont make it into the cache get deleted
    if(!ret) {
        delete page->robots;
        delete page;
    }
}
