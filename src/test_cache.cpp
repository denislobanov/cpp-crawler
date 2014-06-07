#include <iostream>
#include <vector>
#include <sstream>

#include "page_data.hpp"
#include "cache.hpp"

#define MAX_PAGES (2*CACHE_MAX)

using std::cout;
using std::endl;

int main(void)
{
    cache<page_data_c> test_cache;
    std::ostringstream oss;
    int i;

    std::cout<<"filling cache with "<<MAX_PAGES<<" entires"<<std::endl;
    for(i = 0; i < MAX_PAGES; ++i) {
        //page is allocated here, addr stored in memory (cache), then
        //deleted by next clause
        page_data_c* test_page = new page_data_c;

        //generate test url
        oss.str("");
        oss<<"http://test_url_"<<i<<".com/test_page"<<i<<".html";
        std::string test_url = oss.str();

        //create a blank page & fill it with test data
        test_page->rank = i;
        oss.str("");
        oss<<"test_cache.cpp generated page "<<i;
        test_page->description = oss.str();

        //send to cache
        cout<<"adding page "<<i<<" to cache"<<endl;
        test_cache.put_object(test_page, test_url);
        cout<<endl;
    }

    //now any pages that have not make it to cache or have been kicked out
    //have been deleted by the cache class, so we only need to delete
    //the remaining.
    cout<<"\n~~~\ngetting "<<i<<" pages"<<endl;
    for(i = 0; i < MAX_PAGES; ++i) {
        //generate test url
        oss.str("");
        oss<<"http://test_url_"<<i<<".com/test_page"<<i<<".html";
        std::string test_url = oss.str();

        //retrieve test page
        page_data_c* get_test_page;
        if(test_cache.get_object(&get_test_page, test_url)) {
            cout<<"page "<<i<<" rank "<<get_test_page->rank<<endl;
            cout<<"page "<<i<<" description ["<<get_test_page->description<<"]\n"<<endl;
            delete get_test_page;
        } else {
            cout<<"page "<<i<<" not in cache\n"<<endl;
        }
    }

    return 0;
}
