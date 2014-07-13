#if !defined (PAGE_DATA_H)
#define PAGE_DATA_H

#include <vector>
#include <mutex>
#include <glibmm/ustring.h>
#include <chrono>
#include <atomic>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/binary_object.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/string.hpp>
#include "debug.hpp"

/**
 * describes an entry to pass to caching/db mechanism
 */
class page_data_c
{
    public:
    friend class boost::serialization::access;

    page_data_c(void)
    {
        use_count = 0;
    }

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

    void lock(void)
    {
        access_lock.lock();
        ++use_count;            //should only be bool.
    }

    void unlock(void)
    {
        access_lock.unlock();
        if(use_count)
            --use_count;
    }

    bool is_locked(void)
    {
        dbg<<"use count is "<<use_count<<std::endl;
        return use_count > 0;
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        // commented out fields do not appear to compile, but object fully serealizes regardless
        ar & rank;
        ar & crawl_count;
        ar & boost::serialization::make_binary_object(&last_crawl, sizeof(last_crawl));
        ar & out_links;
        ar & boost::serialization::make_binary_object((void*)url.data(), url.bytes());
        ar & boost::serialization::make_binary_object((void*)title.data(), title.bytes());
        ar & boost::serialization::make_binary_object((void*)description.data(), description.bytes());
        ar & boost::serialization::make_binary_object((void*)meta.data(), meta.size());
    };

    private:
    std::mutex access_lock; //only one thread may access at a time, managed by cache class
    std::atomic<unsigned int> use_count;
};

#endif
