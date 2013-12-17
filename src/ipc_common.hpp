#if !defined (IPC_COMMON_H)
#define IPC_COMMON_H

#include <glibmm/ustring.h>
#include <type_traits>

#include "page_data.hpp"

#define MASTER_SERVICE_NAME "23331"
#define MASTER_SERVICE_PORT 23331

/**
 * status of crawler_worker
 */
enum worker_status {
    ZOMBIE,         //dead
    IDLE,           //waiting for instructions
    READY,          //configured
    ACTIVE,         //working
    SLEEP,          //blocked (queue)
    STOPPING        //will idle when current crawls complete
};

/**
 * CnC intructions to workers
 */
enum cnc_instruction {
    w_register,     //worker requests config
    w_get_work,     //worker requests work
    w_send_work,    //worker sending completed work
    m_send_status,  //master requests worker status
};

/**
 * worker config/registration data
 */
enum tag_type_e {            //part of parser configuration
    invalid,
    url,
    title,
    description,
    meta,
    email,
    image
};

struct tagdb_s {            //part of parser configuration
    tag_type_e tag_type;    //meta

    Glib::ustring xpath;    //xpath to match node
    Glib::ustring attr;     //needed to extract attr_data from libxml2
};


struct worker_config {
    std::string user_agent;         //to report to sites
    unsigned int day_max_crawls;    //per page
    unsigned int worker_id;

    //cache config
    unsigned int page_cache_max;
    unsigned int page_cache_res;
    unsigned int robots_cache_max;
    unsigned int robots_cache_res;

    //database
    std::string db_path;            //uri

    //parser
    std::vector<struct tagdb_s> parse_param;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & day_max_crawls;
        ar & worker_id;
        ar & page_cache_max;
        ar & page_cache_res;
        ar & robots_cache_max;
        ar & robots_cache_res;
        ar & db_path;
        ar & parse_param;
    }
};

/**
 * capabilities reported by worker
 */
struct capabilities {
    unsigned int parsers;           //parser threads
    unsigned int total_threads;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & parsers;
        ar & total_threads;
    }
};
#endif
