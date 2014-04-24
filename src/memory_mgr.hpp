#include <iostream>
#include <stdexcept>

#include "page_data.hpp"

class cache;
class robots_txt;

/**
 * generic exception interface to memory_mgr
 *
 * TODO: create a universal crawler_exception?
 */
struct memory_exception: std::exception {
    std::string message;
    const char* what() const noexcept
    {
        return message.c_str();
    }
    memory_exception(std::string s): message(s) {};
};

/**
 * controls allocating/deletion of page_data_c'
 *      for now only uses new/delete
 *
 * to do/
 *      use a pool of pre allocated page_data_c
 *          -- fifo?
 *      get/put to from pool
 */
class memory_mgr
{
    public:
    memory_mgr(std::string database_path, std::string user_agent);
    ~memory_mgr(void);

    page_data_c* get_page(std::string& url) throw(std::exception);
    void put_page(page_data_c* page, std::string& url);
    void free_page(page_data_c* page, std::string& url);

    robots_txt* get_robots_txt(std::string& url);
    void put_robots_txt(robots_txt* robots, std::string& url);

    private:
    cache* mem_cache;
    database* mem_db;
    std::string agent_name;
};
