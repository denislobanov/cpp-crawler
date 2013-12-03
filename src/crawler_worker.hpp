#if !defined (CRAWLER_PROCESS_H)
#define CRAWLER_PROCESS_H

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

class crawler_worker
{
    public:
    /**
     * url_fifo will only be written to. parent process reads work
     * from fifo to crawler instances
     */
    crawler_worker();
    //development version
    crawler_worker(std::vector<struct tagdb_s>& parse_param);
    ~crawler_worker(void);

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
    enum worker_status status;
    struct worker_config config;
    std::string data;
    ipc_client ipc;

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
