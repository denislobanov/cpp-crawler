#include <iostream>
#include <stdexcept>

#include "memory_mgr.hpp"
#include "cache.hpp"
#include "file_db.hpp"
#include "page_data.hpp"

//Local defines
//~ #define DEBUG 5

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
    //currently frees all cached memory, should be handled by this class instead.
    delete mem_cache;
    delete mem_db;

    //store unordered map of all allocated data. free all pointers.
}

page_data_c* memory_mgr::get_page(std::string& url) throw(std::exception)
{
    page_data_c* page;

    //if cache fails to retrieve page, get it from the db
    dbg<<"trying to get page from cache\n";
    if(!mem_cache->get_page_data(&page, url)) {
        dbg<<"page not in cache, trying database\n";
        page = new page_data_c;
        mem_db->get_page_data(page, url);
    } else {
        dbg<<"FIXME: memory_mgr needs to check if cached page is in sync with db.\n";
    }

    //any page returned from memory_mgr is accessible by only one worker thread
    //FIXME: database class must perform similar locking!
    if(!page->access_lock.try_lock())
        throw memory_exception("memory_mgr - cannot lock page, try later");

    //if page exists in cache or db it will be filled with stored data,
    //otherwise we return a blank page.
    return page;
}

void memory_mgr::put_page(page_data_c* page, std::string& url)
{
    mem_db->put_page_data(page, url);
    bool ret = mem_cache->put_page_data(page, url);

    //pages that dont make it into the cache get deleted
    if(!ret) {
        dbg<<"page did not make it to cache, deleting\n";
        delete page;
    } else {
        //unlock page for future access.
        page->access_lock.unlock();
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
    } else {
        dbg<<"FIXME: memory_mgr needs to check if cached robots_txt is in sync with db.\n";
    }

    ++robots->use_count;

    //if it exists in cache or db it will be filled with stored data,
    //otherwise we return a new robots_txt object
    return robots;
}

void memory_mgr::put_robots_txt(robots_txt* robots, std::string& url)
{
    mem_db->put_robots_txt(robots, url);
    bool ret = mem_cache->put_robots_txt(robots, url);

    //free object only if no-one else is using it
    --robots->use_count;
    if((robots->use_count == 0) && !ret) {
        dbg<<"object did not make it to cache, deleting\n";
        delete robots;
    }
}

//deallocates memory used for page in cache and removes it from the database
void memory_mgr::free_page(page_data_c* page, std::string& url)
{
    dbg<<"freeing page\n";

    //remove page from database
    mem_db->rm_page_data(page, url);

    //remove cache entry
    mem_cache->rm_page_data(page, url);

    //free memory
    delete page;
}


