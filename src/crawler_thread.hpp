#if !defined (CRAWLER_THREAD_H)
#define CRAWLER_THREAD_H

#include <iostream>
#include <vector>
#include <stdexcept>

#include "page_data.hpp"
#include "ipc_common.hpp"
#include "parser.hpp"
#include "ipc_client.hpp"

class netio;
class memory_mgr;
class ipc_client;
class robots_txt;

/**
 * Global, part of objects interface
 */
#define ROBOTS_REFRESH  60*5   //seconds

class crawler_thread
{
    public:
    /**
     * url_fifo will only be written to. parent process reads work
     * from fifo to crawler instances
     */
    crawler_thread();
    //development version
    crawler_thread(std::vector<struct tagdb_s>& parse_param);
    ~crawler_thread(void);

    /**
     * crawl #i items from queue. development implementation.
     *
     * urls found during the crawl are automattically appended to queue specified on object creation
     */
    void dev_loop(int i) throw (std::underflow_error); //may sleep

    /**
     * write me, should throw exceptions
     */
    void main_loop(void);

    private:
    enum worker_status _status;
    struct worker_config config;
    std::string data;

    //objects dynamically allocated based on config
    netio* netio_obj;
    memory_mgr* mem_mgr;

    size_t root_domain(std::string& url);
    bool sanitize_url_tag(struct data_node_s& d, std::string root_url);
    unsigned int tokenize_meta_tag(struct page_data_s* page, Glib::ustring& data);
    bool is_whitespace(Glib::ustring::value_type c);
    void crawl(queue_node_s& work_item, struct page_data_s* page, robots_txt* robots);
};

#endif
