#if !defined (IPC_COMMON_H)
#define IPC_COMMON_H

#include <glibmm/ustring.h>

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
enum worker_intruction {
    START,          //begin processing data from work queue
    STOP,           //finish current processing and idle
    CAPABILITIES,   //report capabilities
    CONFIGURE,      //configure worker
    GET_CONF,       //report current config
    KILL,           //die now
    NO_INST         //no instruction
};

/**
 * configuration sent to worker
 */
enum tag_type_e {
    invalid,
    url,
    title,
    description,
    meta,
    email,
    image
};

struct tagdb_s {
    tag_type_e tag_type;    //meta

    Glib::ustring xpath;    //xpath to match node
    Glib::ustring attr;     //needed to extract attr_data from libxml2
};

struct worker_config {
    std::string user_agent;
    unsigned int day_max_crawls;

    //cache config
    unsigned int page_cache_max;
    unsigned int page_cache_res;
    unsigned int robots_cache_max;
    unsigned int robots_cache_res;

    //database
    std::string db_path;

    //parser
    std::vector<struct tagdb_s> parse_param;
};

/**
 * capabilities reported by worker
 */
struct capabilities {
    unsigned int parsers;
    unsigned int total_threads;
};

#endif
