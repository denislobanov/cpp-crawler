#if !defined (CRAWLER_THREAD_H)
#define CRAWLER_THREAD_H

#include <iostream>
#include <vector>
#include <stdexcept>
#include <thread>

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
#define ROBOTS_REFRESH  15*60   //15 minutes

class crawler_thread
{
    public:
    crawler_thread(ipc_client* ipc_obj);
    ~crawler_thread(void);

    /**
     * starts the internal crawler thread. Will block until crawler configuration
     * data has been recieved via the ipc_obj
     */
    void start(void);

    /**
     * starts the internal crawler thread using the given configuration.
     * This will not block, but internal configuration is periodically
     * refreshed via the ipc_obj
     */
    void start(worker_config& config);

    /**
     * signals the internal thread to shut down once its completed its
     * current crawl.
     *
     * Returns immidiately
     */
    void stop(void);

    /**
     * Returns current status of internal crawler thread
     */
    worker_status status(void);

    private:
    worker_status _status;
    struct worker_config cfg;
    std::string data;
    std::thread main_thread;

    //objects dynamically allocated based on config
    netio* netio_obj;
    memory_mgr* mem_mgr;
    ipc_client* ipc;

    size_t root_domain(std::string& url);
    void crawl(queue_node_s& work_item, struct page_data_s* page, robots_txt* robots);
    void thread() throw(std::underflow_error);
    unsigned int tax(unsigned int credit, unsigned int percent);
    void launch_thread(void);
    bool sanitize_url_tag(struct data_node_s& d, std::string root_url);
    bool is_whitespace(Glib::ustring::value_type c);
    unsigned int tokenize_meta_tag(struct page_data_s* page, Glib::ustring& data);
};

#endif
