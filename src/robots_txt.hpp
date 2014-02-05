#if !defined (ROBOTS_TXT_H)
#define ROBOTS_TXT_H

#include <iostream>
#include <ctime>
#include <vector>
#include <atomic>

class netio;

/**
 * if the time between visits is greater than this, page should be requeued
 * without crawling
 */
#define REVISIT_TOO_LONG    1000

/**
 * in the venet that robots.txt does not provide a Crawl-delay: command, use this
 */
#define DEFAULT_CRAWL_DELAY 60  //seconds

#define MAX_DATA_SIZE 500*1024  //500k

class robots_txt
{
    friend class memory_mgr;

    public:
    /**
     * creates a robots_txt parser instance.
     *
     * netio used only during object creation and by optionall parser() call
     * crawler_name is needed to check "User-agent:" field of robots.txt
     * root_domain will have robots.txt appended automatically
     */
    robots_txt(std::string user_agent, std::string root_domain);
    ~robots_txt(void);

    /**
     * optional call to refresh current robots.txt profile
     */
    void fetch(netio& netio_obj);

    /**
     * checks if path (usually url) is within the exclusion list
     * (robots.txt "Dissalow: ")
     */
    bool exclude(std::string& path);

    /**
     * how long to wait before recrawls
     */
    time_t crawl_delay;

    /**
     * when the page was last visited
     */
    time_t last_visit;

    /**
     * returns true if sitemap present, data set to sitemap url
     */
    bool sitemap(std::string& data);
    
    /**
     * avoid excessive calls to get robots_txt on site previously visited but not cache
     * returns true if copy occured
     *      data should be ignored otherwise
     */
    bool export_exclusions(std::vector<std::string>& data);
    bool export_inclusions(std::vector<std::string>& data);
    
    /**
     * counterpart to above
     */
    void import_exclusions(std::vector<std::string>& data);
    void import_inclusions(std::vector<std::string>& data);

    private:
    bool can_crawl; //if crawler's completely banned or a whitelist policy is used
    bool process_param;
    //used as a semaphore for freeing memory via memory_mgr
    std::atomic<unsigned int> use_count;

    std::string agent_name;
    std::string domain;
    std::vector<std::string> disallow_list;
    std::vector<std::string> allow_list;
    std::string sitemap_url;

    void parse(std::string& data);
    size_t line_is_comment(std::string& data);
    bool get_param(std::string& lc_data, size_t& pos, size_t& eol, std::string param);
    void process_instruction(std::string& data, std::string& lc_data, size_t pos, size_t eol);
    void sanitize(std::string& data, std::string bad_char);
};

#endif
