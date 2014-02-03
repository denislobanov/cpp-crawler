#if !defined(DATABASE_H)
#define DATABASE_H

#include <iostream>
#include <functional>

#include "page_data.hpp"
#include "robots_txt.hpp"

class database
{
    public:
    database(std::string path);
    ~database(void);

    /**
     * url is hashed and a page_data_s retrieved.
     * if no page_data_s is present, a new one is allocated.
     */
    void get_page_data(struct page_data_s* page_data, std::string& url);

    /**
     * page is potentially cached, depending on criteria and sent to the db
     */
    void put_page_data(struct page_data_s* page_data, std::string& url);

    /**
     * removes a page from database
     */
    void rm_page_data(struct page_data_s* page_data, std::string& url);

    /**
     * checks if the data in memory is in sync with that in the database
     * returns true if it is.
     */
    bool page_in_sync(struct page_data_s* page_data, std::string& url);
    
    /**
     * same API for robots_txt as page_data_s
     */
    void get_robots_txt(robots_txt* robots, std::string& url);
    void put_robots_txt(robots_txt* robots, std::string& url);

    private:
    std::string db_path;
    std::mutex file_io_lock;    //concurrent open to the same file

    bool is_inst(std::string line, std::string inst);
    void store_keywords(struct page_data_s* page_data, std::string page_hash);
};

#endif
