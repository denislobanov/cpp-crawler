#include <iostream>
#include <vector>
#include <string>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <glibmm/ustring.h>
#include <glib.h>
#include <glibmm/convert.h>

#include "parser.hpp"
#include "ipc_common.hpp"

//Local defines
//~ #define DEBUG 0

#if (defined(DEBUG))&&(DEBUG > 2)
#include <fstream>
#endif

#if defined(DEBUG)
    #define dbg std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #if DEBUG > 1
        #define dbg_2 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_2 0 && std::cout
    #endif
#else
    #define dbg 0 && std::cout
    #define dbg_2 0 && std::cout
#endif

parser::parser(Glib::ustring url)
{
    doc_url = url;

    //hardwired config
    int h_opt = HTML_PARSE_RECOVER|HTML_PARSE_NOERROR|HTML_PARSE_NOWARNING|HTML_PARSE_NOBLANKS;
    doc = htmlReadFile(url.c_str(), 0, h_opt);
}

parser::~parser(void)
{
    xmlFreeDoc(doc);
    xmlCleanupParser();
}

void parser::save_nodes(struct tagdb_s& param)
{
    xmlNodeSetPtr node_set = tags->nodesetval;
    dbg_2<<"saving nodes\n";

    for(int i = 0; i < node_set->nodeNr; ++i) {
        struct data_node_s data_entry;

        //tag_name
        data_entry.tag_name = reinterpret_cast<const char*>(node_set->nodeTab[i]->name);

        //tag_data
        xmlChar *tmp = xmlNodeListGetString(doc, node_set->nodeTab[i]->children, 1);
        if(tmp > 0)
            data_entry.tag_data = reinterpret_cast<const char*>(tmp);
        xmlFree(tmp);

        //attr_data
        if(!param.attr.empty()) {
            dbg_2<<"saving attr ["<<param.attr<<"] data\n";
            tmp = xmlGetProp(node_set->nodeTab[i], reinterpret_cast<const xmlChar*>(param.attr.c_str()));
            data_entry.attr_data = reinterpret_cast<const char *>(tmp);
            xmlFree(tmp);
        }

        //(expected)tag_type
        data_entry.tag_type = param.tag_type;

        try {
            dbg_2<<"tag name ["<<data_entry.tag_name<<"] tag data ["<<data_entry.tag_data<<"] attr_data ["<<data_entry.attr_data<<"]\n";
        } catch(Glib::ConvertError& e) {
            std::cerr<<"Glb conversion error: "<<e.what();
        }
        data.push_back(data_entry);
    }
}

void parser::parse(std::vector<struct tagdb_s>& param)
{
    if(doc) {
        //succesfuly parsed
        dbg<<"doc valid\n";
        xmlXPathContextPtr xpath_ctxt = xmlXPathNewContext(doc);
        if(xpath_ctxt) {
            dbg_2<<"created xpath context, processing tags\n";

            //parse xpath tags
            for(auto& p: param) {
                dbg_2<<"matching xpath ["<<p.xpath<<"]"<<std::endl;

                tags = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(p.xpath.c_str()), xpath_ctxt);
                if(!xmlXPathNodeSetIsEmpty(tags->nodesetval)) {
                    save_nodes(p);
                }
                xmlXPathFreeObject(tags);
            }
            xmlXPathFreeContext(xpath_ctxt);
        } else {
            //FIXME: raise exception
            std::cerr<<"Failed to create xpath_ctxt\n";
        }
    } else {
        //raise exception
        std::cerr<<"Failed to succesfully parse "<<doc_url<<std::endl;
    }
}
