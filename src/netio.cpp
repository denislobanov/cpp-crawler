#include <iostream>
#include <mutex>
#include <vector>
#include <curl/curl.h>

#include "netio.hpp"
#include "debug.hpp"

netio::netio(std::string user_agent_string)
{
    user_agent = user_agent_string;

    //initialise libCURL
    lib_handle = curl_easy_init();
    default_config(false);
}

netio::netio(std::string user_agent_string, bool enable_debug)
{
    user_agent = user_agent_string;

    //initialise libCURL
    lib_handle = curl_easy_init();
    default_config(enable_debug);
}

netio::~netio(void)
{
    curl_easy_cleanup(lib_handle);
}

bool netio::fetch(std::string* mem, std::string url)
{
    mem->clear(); //parsing implies new data
    dbg<<"fetching ["<<url<<"]\n";

    //locking is needed as lib_handle cannot be used simmultaniously
    //both setting url and retrieving data must be locked for data to
    //coincide with url
    lib_mutex.lock();
    target_memory = mem;

    curl_easy_setopt(lib_handle, CURLOPT_URL, url.c_str());
    curl_ret = curl_easy_perform(lib_handle);
    lib_mutex.unlock();

    dbg<<"netio: size of data retrieved: "<<target_memory->size()<<std::endl;

    return (curl_ret == CURLE_OK)?true:false;
}

std::string netio::last_error(void)
{
    return error_buffer;
}

//internal callback method wrapper (libCURL expects a C function)
static size_t store_data_callback(char *ptr, size_t size, size_t nmemb, void *userp)
{
    if(userp)
        return ((netio*)userp)->store_data(ptr, size, nmemb);
    else
        return 0;
}

bool netio::default_config(bool debug)
{
    curl_ret = curl_easy_setopt(lib_handle, CURLOPT_VERBOSE, ((debug == true)?1:0));
    if(curl_ret != CURLE_OK)
        return false;

    //options
    if((curl_ret = curl_easy_setopt(lib_handle, CURLOPT_HEADER, 0)) != CURLE_OK)
        return false;
    if((curl_ret = curl_easy_setopt(lib_handle, CURLOPT_NOPROGRESS, 1)) != CURLE_OK)
        return false;
    if((curl_ret = curl_easy_setopt(lib_handle, CURLOPT_NOSIGNAL, 1)) != CURLE_OK)
        return false;
    if((curl_ret = curl_easy_setopt(lib_handle, CURLOPT_USERAGENT, user_agent.c_str())) != CURLE_OK)
        return false;

    //callbacks
    if((curl_ret = curl_easy_setopt(lib_handle, CURLOPT_WRITEFUNCTION, &store_data_callback)) != CURLE_OK)
        return false;
    if((curl_ret = curl_easy_setopt(lib_handle, CURLOPT_WRITEDATA, this)) != CURLE_OK)
        return false;

    return true;
}

void netio::reset_config(void)
{
    curl_easy_reset(lib_handle);
}

size_t netio::store_data(char *ptr, size_t size, size_t nmemb)
{
    size_t real_size = size*nmemb;

    target_memory->append(ptr, real_size);
    return real_size;
}
