#include <iostream>
#include <ctime>
#include <vector>

#include "robots_txt.hpp"
#include "netio.hpp"

int main(void)
{
    netio my_netio("lcpp robots_txt unit test");
    robots_txt my_robots_txt("sdfsdfsdf", "www.geeksaresexy.net");
    //~ robots_txt my_robots_txt(my_netio, "lcpp robots_txt unit test", "http://en.wikipedia.org");
    my_robots_txt.fetch(my_netio);

    //test crawl-delay
    unsigned int i = (unsigned int)my_robots_txt.crawl_delay;
    std::cout<<"crawl delay: "<<i<<std::endl;

    //test disallow/allow control (plu export of disallow_list)
    std::cout<<"exclusions list:"<<std::endl;
    std::vector<std::string> exclusions_list;
    my_robots_txt.export_exclusions(exclusions_list);
    for(std::vector<std::string>::iterator it = exclusions_list.begin(); it != exclusions_list.end(); ++it)
        std::cout<<*it<<std::endl;

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
