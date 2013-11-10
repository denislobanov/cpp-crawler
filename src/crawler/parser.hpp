#if !defined(PARSER_H)
#define PARSER_H

#include <iostream>
#include <vector>
#include <libxml/HTMLparser.h>  //libxml2
#include <libxml/xpath.h>
#include <glibmm/ustring.h>

#include "ipc_common.hpp"

//data returned from crawl
struct data_node_s {
    Glib::ustring tag_name;    //tag name crawled
    Glib::ustring tag_data;    //data contained within tag
    Glib::ustring attr_data;   //value of each tag attribute
    tag_type_e tag_type;       //type of tag that this /should/ be
};

/* to do
 * give parser netio object on creation
 *  - libcurl is needed for https access (and others?)
 *  - see libxml2 nanohttp limitations
 * pass tagdb_s config to parse()
 *  - no need to copy config each time
 */



class parser
{
    public:
    parser(Glib::ustring url);
    ~parser(void);

    //walks the document tree, parsing based on configuration
    void parse(std::vector<struct tagdb_s>& param);

    //data from parsing
    std::vector<struct data_node_s> data;

    private:
    std::string doc_url;

    //libxml2
    htmlDocPtr doc;
    xmlXPathObjectPtr tags;

    void save_nodes(struct tagdb_s& param);
};

#endif
