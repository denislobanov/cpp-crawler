#if !defined (PAGE_DATA_H)
#define PAGE_DATA_H

#include <vector>
#include <mutex>
#include <glibmm/ustring.h>
#include <chrono>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/binary_object.hpp>

/**
 * describes an entry to pass to caching/db mechanism
 */
class page_data_c
{
    public:
    friend class boost::serialization::access;

    //page meta data
    unsigned int rank;
    unsigned int crawl_count;
    std::chrono::system_clock::time_point last_crawl;
    std::vector<std::string> out_links;
    Glib::ustring url;

    //descriptive
    Glib::ustring title;              //page title
    Glib::ustring description;        //short blob about page
    std::vector<Glib::ustring> meta;  //keywords associated with page

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & rank;
        ar & crawl_count;
        //~ ar & boost::serialization::make_binary_object(&last_crawl, sizeof(last_crawl));
        ar & out_links;
        //~ ar & url.raw();
        //~ ar & title.raw();
        //~ ar & description.raw();
        //~ ar & boost::serialization::make_binary_object(meta.data(), meta.size());
    }

    private:
    friend class cache;
    std::mutex access_lock; //only one thread may access at a time, managed by cache class
};

/**
 * describes an entry in the crawl (IPC) queue
 */
struct queue_node_s {
    unsigned int credit;    //cash given to link from referring page
    std::string url;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & credit;
        ar & url;
    }
};

#endif
