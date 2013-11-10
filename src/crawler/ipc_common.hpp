#if !defined (IPC_COMMON_H)
#define IPC_COMMON_H

#include <glibmm/ustring.h>

#define CNC_SERVICE_NAME "worker_cnc"
#define TO_WORKER_SERVICE_NAME "work_in"
#define FROM_WORKER_SERVICE_NAME "work_out"

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
    KILL,           //die now
    STATUS,         //report current status
    NO_INST         //no instruction
};

enum master_instruction {
    BOOTSTRAP,      //provide initial configuration
    GET_WORK,       //request work items
    NO_INST         //no request, checking in
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
