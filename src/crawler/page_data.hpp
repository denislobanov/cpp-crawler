#if !defined (PAGE_DATA_H)
#define PAGE_DATA_H

#include <vector>
#include <mutex>
#include <glibmm/ustring.h>
#include <chrono>


/**
 * describes an entry to pass to caching/db mechanism
 */
struct page_data_s {
    //meta
    unsigned int rank;
    unsigned int crawl_count;
    std::chrono::system_clock::time_point last_crawl;
    std::vector<std::string> out_links;

    //descriptive
    Glib::ustring title;              //page title
    Glib::ustring description;        //short blob about page
    std::vector<Glib::ustring> meta;  //keywords associated with page

    //house keeping
    std::mutex access_lock; //only one thread may access at a time, managed by cache class
};

/**
 * describes an entry in the crawl (IPC) queue
 */
struct queue_node_s {
    unsigned int credit;    //cash given to link from referring page
    std::string url;
};

#endif
