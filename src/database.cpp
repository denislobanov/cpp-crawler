#include <iostream>
#include <functional>
#include <fstream>
#include <sstream>
#include <glibmm/ustring.h>
#include <chrono>
#include <glib.h>

#include "database.hpp"
#include "page_data.hpp"
#include "robots_txt.hpp"

//Local defines
//~ #define DEBUG 2

#if defined(DEBUG)
    #define dbg std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #if DEBUG > 1
        #define dbg_1 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_1 0 && std::cout
    #endif
#else
    #define dbg 0 && std::cout
    #define dbg_1 0 && std::cout
#endif

#define escape_string(s) ::g_uri_escape_string(s.c_str(), 0, true)

#define unescape_string(s) ::g_uri_unescape_string(s.c_str(), 0)


database::database(std::string uri) throw(std::exception)
{
    try {
        c.connect(uri);
    } catch(const mongo::DBException &e) {
        throw db_exception("could not connect to mongoDB \""+path+"\"");
    }
}

database::~database(void) {}

void database::void put_object(T* t, std::string& key)
{
    
}
