#if !defined (IPC_COMMON_H)
#define IPC_COMMON_H

#include <glibmm/ustring.h>
#include <type_traits>
#include <boost/serialization/vector.hpp>

#define MASTER_SERVICE_NAME "23331"
#define MASTER_SERVICE_PORT 23331

/**
 * status of crawler_thread
 */
enum worker_status {
    ZOMBIE,         //dead
    STOP,           //will die when current crawls complete
    IDLE,           //waiting for instructions
    ACTIVE,         //working
    SLEEP,          //blocked (queue)
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
    tag_type_invalid,
    tag_type_url,
    tag_type_title,
    tag_type_description,
    tag_type_meta,
    tag_type_email,
    tag_type_image
};

struct tagdb_s {            //part of parser configuration
    tag_type_e tag_type;    //meta

    //FIXME: use Glib::ustring here - as xpaths need to be unicode
    std::string xpath;    //xpath to match node
    std::string attr;     //needed to extract attr_data from libxml2

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & tag_type;
        ar & xpath;
        ar & attr;
    }
};

struct worker_config {
    friend class boost::serialization::access;

    std::string user_agent;         //to report to sites
    unsigned int day_max_crawls;    //per page
    unsigned int worker_id;

    //cache config
    unsigned int page_cache_max;
    unsigned int page_cache_res;
    unsigned int robots_cache_max;
    unsigned int robots_cache_res;

    //database
    std::string db_path;
    std::string page_table;
    std::string robots_table;

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
        ar & page_table;
        ar & robots_table;
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

/**
 * Type of data sent in an ipc message
 */
enum data_type_e {
    instruction,
    cnc_data,
    queue_node
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
