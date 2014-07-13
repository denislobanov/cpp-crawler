#if !defined (PAGE_DATA_H)
#define PAGE_DATA_H

#include <vector>
#include <mutex>
#include <glibmm/ustring.h>
#include <chrono>
#include <atomic>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/binary_object.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/base_object.hpp>
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
    void save(Archive& ar, const unsigned int version) const
    {
        ar << boost::serialization::base_object<const page_data_c>(*this);

        ar << rank;
        ar << crawl_count;
        ar << boost::serialization::make_binary_object(&last_crawl, sizeof(last_crawl));
        ar << out_links;

        std::string url_str = url.raw();
        std::string title_str = title.raw();
        std::string description_str = description.raw();
        ar << url_str;
        ar << title_str;
        ar << description_str;

        std::vector<std::string> meta_raw;
        for(int i = 0;i<meta.size();++i) {
            meta_raw.push_back(meta.at(i).raw());
        }
        ar << meta_raw;
    };

    template<class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        ar >> boost::serialization::base_object<page_data_c>(*this);

        ar >> rank;
        ar >> crawl_count;
        ar >> boost::serialization::make_binary_object(&last_crawl, sizeof(last_crawl));
        ar >> out_links;

        std::string url_raw;
        std::string title_raw;
        std::string description_raw;
        ar >> url_raw;
        ar >> title_raw;
        ar >> description_raw;
        url = url_raw;
        title = title_raw;
        description = description_raw;

        std::vector<std::string> meta_raw;
        ar >> meta_raw;
        for(auto x: meta_raw)
            meta.push_back(x);
    };

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    private:
    std::mutex access_lock; //only one thread may access at a time, managed by cache class
    std::atomic<unsigned int> use_count;
};

#endif
