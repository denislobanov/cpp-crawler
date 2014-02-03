#include <iostream>

#include "page_data.hpp"

class cache;
class database;
class robots_txt;

/**
 * controls allocating/deletion of page_data_s'
 *      for now only uses new/delete
 *
 * to do/
 *      use a pool of pre allocated page_data_s
 *          -- fifo?
 *      get/put to from pool
 */
class memory_mgr
{
    public:
    memory_mgr(std::string database_path, std::string user_agent);
    ~memory_mgr(void);

    struct page_data_s* get_page(std::string& url);
    void put_page(struct page_data_s* page, std::string& url);
    void free_page(struct page_data_s* page, std::string& url);

    robots_txt* get_robots_txt(std::string& url);
    void put_robots_txt(robots_txt* robots, std::string& url);

    private:
    cache* mem_cache;
    database* mem_db;
    std::string agent_name;
};
