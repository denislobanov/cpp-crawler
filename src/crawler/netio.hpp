#ifndef NETIO_H
#define NETIO_H

#include <iostream>
#include <mutex>
#include <curl/curl.h>

class netio
{
    public:
    netio(std::string user_agent_string);
    netio(std::string user_agent_string, bool enable_debug);
    ~netio();

    bool fetch(std::string* mem, std::string url);
    std::string last_error(void);
    void reset_config(void);
    size_t store_data(char *ptr, size_t size, size_t nmemb);

    private:
    std::mutex lib_mutex;
    CURL* lib_handle;
    CURLcode curl_ret;

    std::string user_agent;
    std::string error_buffer;
    std::string* target_memory;

    bool default_config(bool debug);
};

#endif
