#include <iostream>
#include <vector>
#include <sstream>

#include "page_data.hpp"
#include "cache.hpp"

using std::cout;
using std::endl;

int main(void)
{
    cache test_cache;
    std::ostringstream oss;
    int i;

    struct page_data_s *test_page;

    //test ctl
    const int max_pages = 2*PAGE_CACHE_MAX;

    std::cout<<"filling cache with "<<max_pages<<" entires"<<std::endl;
    for(i = 0; i < max_pages; ++i) {
        //generate test url
        oss.str("");
        oss<<"http://test_url_"<<i<<".com/test_page"<<i<<".html";
        std::string test_url = oss.str();

        //create a blank page & fill it with test data
        test_page = new struct page_data_s;
        test_page->rank = i;
        oss.str("");
        oss<<"test_cache.cpp generated page "<<i;
        test_page->description = oss.str();

        //send to cache
        cout<<"adding page "<<i<<" to cache"<<endl;
        test_cache.put_page_data(test_page, test_url);
        test_page->access_lock.unlock();    //normally done by memory_mgr class
        cout<<endl;
    }

    cout<<"\n~~~\ngetting "<<i<<" pages"<<endl;
    for(i = 0; i < max_pages; ++i) {
        //generate test url
        oss.str("");
        oss<<"http://test_url_"<<i<<".com/test_page"<<i<<".html";
        std::string test_url = oss.str();

        //retrieve test page
        struct page_data_s *get_test_page;
        if(test_cache.get_page_data(&get_test_page, test_url)) {
            cout<<"page "<<i<<" rank "<<get_test_page->rank<<endl;
            cout<<"page "<<i<<" description ["<<get_test_page->description<<"]\n"<<endl;

        } else {
            cout<<"page "<<i<<" not in cache\n"<<endl;
        }
    }

    return 0;
}
