#include <iostream>
#include <ctime>
#include <vector>

#include "robots_txt.hpp"
#include "netio.h"

int main(void)
{
    netio my_netio("lcpp robots_txt unit test");
    robots_txt my_robots_txt(my_netio, "lcpp robots_txt unit test", "www.geeksaresexy.net");

    unsigned int i = (unsigned int)my_robots_txt.crawl_delay();
    std::cout<<"crawl delay: "<<i<<std::endl;
    std::cout<<"exclusions list:"<<std::endl;

    std::vector<std::string> exclusions_list;
    my_robots_txt.export_exclusions(exclusions_list);
    for(std::vector<std::string>::iterator it = exclusions_list.begin(); it != exclusions_list.end(); ++it)
        std::cout<<*it<<std::endl;
    
    return 0;
}