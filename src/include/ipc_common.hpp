#if !defined (IPC_COMMON_H)
#define IPC_COMMON_H

#include <glibmm/ustring.h>
#include <type_traits>
#include <boost/serialization/vector.hpp>

#define MASTER_SERVICE_NAME "23331"
#define MASTER_SERVICE_PORT 23331

//
//IPC Meta definition
//
/**
 * Type of data sent in an ipc message
 */
enum data_type_e {
    dt_instruction, //ctrl_instruction_e        worker <-> master
    dt_wstatus,     //worker_status_e           worker -> master
    dt_wcap,        //worker_capabilities_s     worker -> master
    dt_wconfig,     //worker_config_s           worker <- master
    dt_queue_node   //queue_node_s              worker <-> master
};

//
// IPC Payload types
//
/**
 * Bidirectional control instructions, can be sent from worker or master.
 */
enum ctrl_instruction_e {
    ctrl_mnoconfig, //master has no config updates or no config present
    ctrl_mstatus,   //master requests worker status
    ctrl_mcap,      //master requests worker capabilities
    ctrl_wconfig,   //worker requests new config (called on register, subsequent polls)
    ctrl_wnodes,    //worker requests queue_nodes_s to process
};

/**
 * Current worker status. Each crawler_thread keeps track of internal status,
 * worker process then forms an aggregate which is sent to master (on request).
 */
enum worker_status_e {
    ZOMBIE,         //dead
    STOP,           //will die when current crawls complete
    IDLE,           //waiting for instructions
    ACTIVE,         //working
    SLEEP,          //blocked (queue)
};

/**
 * Worker capabilities, as reported to master
 */
struct worker_capabilities_s {
    unsigned int parsers;           //parser threads (currently == crawler_threads)
    unsigned int total_threads;     //total threads in worker

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & parsers;
        ar & total_threads;
    }
};

/**
 * Meta description of tag search type, used by hardcoded logic in parser
 * to fill out page_data_c
 *
 * Enum is part of tagdb_s for IPC
 */
enum tag_type_e {
    tag_type_invalid,
    tag_type_url,
    tag_type_title,
    tag_type_description,
    tag_type_meta,
    tag_type_email,
    tag_type_image
};

/**
 * Used to configure page parsing parameters - thing to search for in a
 * page.
 *
 * Struct is part of worker_config_s for IPC
 */
struct tagdb_s {
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

/**
 * Configuration parameters for each crawler_thread. Each worker requests
 * this struct on registration with a master server and will periodically
 * poll for updates.
 */
struct worker_config_s {
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
 * Describes an entry in the crawl (IPC) queue
 */
struct queue_node_s {
    unsigned int credit;    //cash given to link from referring page
    std::string url;        //the url of found page (url to crawl)

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & credit;
        ar & url;
    }
};
#endif
