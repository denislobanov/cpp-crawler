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
};

/**
 * capabilities reported by worker
 */
struct capabilities {
    unsigned int parsers;           //parser threads
    unsigned int total_threads;
};

/**
 * Communication structure
 *  - homologates sending/recieving of CnC & URL data as well as instructions
 */
enum message_type {
    instruction,    //cnc_instruction
    cnc_data,       //worker_config or capabilities depending on previous cnc_instruction
    queue_node      //queue_node_s
};

union ipc_data {
    cnc_instruction instruction;
    worker_status status;
    std::aligned_storage<sizeof(struct worker_config),
        alignof(struct worker_config)>::type config;
    std::aligned_storage<sizeof(struct capabilities),
        alignof(struct capabilities)>::type cap;
    std::aligned_storage<sizeof(struct queue_node_s),
        alignof(struct queue_node_s)>::type node;
};

struct ipc_message {
    message_type type;
    ipc_data data;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & type;
        ar & data;
    }
};

#endif
