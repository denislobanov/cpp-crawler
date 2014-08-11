#include <iostream>
#include <ctime>
#include <vector>

#include "robots_txt.hpp"
#include "netio.hpp"

#define USER_AGENT "test_robots_txt"

int main(void)
{
    netio test_netio(USER_AGENT);
    robots_txt my_robots_txt(USER_AGENT, "www.geeksaresexy.net", &test_netio);
    my_robots_txt->fetch();

    //test crawl-delay
    std::cout<<"crawl delay: "<<my_robots_txt.crawl_delay().count()<<std::endl;

    //test path exclusion
    std::string test_path = "www.geeksaresexy.net/feed/blahblahblah.html";
    std::cout<<"test_path ["<<test_path<<"] ";
    if(my_robots_txt.exclude(test_path))
        std::cout<<"is excluded";
    else
        std::cout<<"is NOT excluded";
    std::cout<<std::endl;

    //test sitemap param
    std::string sitemap_url;
    if(my_robots_txt.sitemap(sitemap_url))
        std::cout<<"sitemap ["<<sitemap_url<<"]"<<std::endl;
    else
        std::cout<<"failed to retrieve sitemap"<<std::endl;

    return 0;
}
