#if !defined (ROBOTS_TXT_H)
#define ROBOTS_TXT_H

#include <chrono>
#include <ctime>
#include <vector>
#include <atomic>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/binary_object.hpp>
#include <boost/serialization/split_member.hpp>

class netio;

/**
 * if the time between visits is greater than this, page should be requeued
 * without crawling
 */
#define REVISIT_TOO_LONG    1000

#define MAX_DATA_SIZE 500*1024  //500k

class robots_txt
{
    friend class boost::serialization::access;
    public:
    /**
     * creates a robots_txt parser instance.
     *
     * netio used only for fetch() call
     * crawler_name is needed to check "User-agent:" field of robots.txt
     * root_domain will have robots.txt appended automatically
     */
    robots_txt(std::string user_agent, std::string root_domain, netio* netio_object);
    robots_txt(void);

    /**
     * set basic configuration parameters if void constructor was used,
     * in such case this method must be called.
     */
    void configure(std::string user_agent, std::string root_domain, netio* netio_object);

    /**
     * optional call to refresh current robots.txt profile
     */
    void fetch(void);

    /**
     * checks if path (usually url) is within the exclusion list
     * (robots.txt "Dissalow: ")
     */
    bool exclude(std::string& path);

    /**
     * how long to wait before recrawls
     */
    std::chrono::seconds crawl_delay(void);

    /**
     * when the page was last visited
     */
    std::chrono::system_clock::time_point last_visit(void);

    /**
     * returns true if sitemap present, data set to sitemap url
     */
    bool sitemap(std::string& data);

    /**
     * returns true whilst use_count > 0
     */
    bool is_locked(void);

    /**
     * increment or decrement use_count, which acts as a semaphore for
     * access count. This is only needed so that robots_txt objects are
     * not deleted whilst in use by another thread.
     */
    void lock(void);
    void unlock(void);

    private:
    bool can_crawl; //if crawler's completely banned or a whitelist policy is used
    bool process_param;
    //used as a semaphre for freeing memory via memory_mgr - not serealized
    std::atomic<unsigned int> use_count;
    netio* netio_obj;

    std::string agent_name;
    std::string domain;
    std::vector<std::string> disallow_list;
    std::vector<std::string> allow_list;
    std::string sitemap_url;
    std::chrono::seconds timeout;
    std::chrono::system_clock::time_point last_access;

    void parse(std::string& data);
    size_t line_is_comment(std::string& data);
    bool get_param(std::string& lc_data, size_t& pos, size_t& eol, std::string param);
    void process_instruction(std::string& data, std::string& lc_data, size_t pos, size_t eol);
    void sanitize(std::string& data, std::string bad_char);

    template<class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        ar << can_crawl;
        ar << process_param;
        ar << agent_name;
        ar << domain;
        ar << disallow_list;
        ar << allow_list;
        ar << sitemap_url;

        //serialize last_access
        time_t tt = std::chrono::system_clock::to_time_t(last_access);
        ar << tt;
        

        //serialize timeout as a delta of two time points
        // t1 = t2 + timeout        -- last_access is t2
        std::chrono::system_clock::time_point t = last_access + timeout;
        tt = std::chrono::system_clock::to_time_t(t);
        ar << tt;
    };

    template<class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        ar >> can_crawl;
        ar >> process_param;
        ar >> agent_name;
        ar >> domain;
        ar >> disallow_list;
        ar >> allow_list;
        ar >> sitemap_url;

        //deserialize last_access
        time_t tt;
        ar >> tt;
        last_access = std::chrono::system_clock::from_time_t(tt);

        //deserialize timeout, inverse of save() thus:
        // timeout = t1 - t2        -- last_access is t2
        ar >> tt;
        std::chrono::system_clock::time_point t = std::chrono::system_clock::from_time_t(tt);
        timeout = std::chrono::duration_cast<std::chrono::seconds>(t - last_access);
    };

    BOOST_SERIALIZATION_SPLIT_MEMBER();
};

#endif
