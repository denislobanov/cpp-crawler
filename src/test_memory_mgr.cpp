#include <iostream>
#include <vector>
#include <sstream>
#include <ctime>

#include "page_data.hpp"
#include "memory_mgr.hpp"

using std::cout;
using std::endl;

int main(void)
{
    memory_mgr test_mgr("./test_db", "test_agent");
    page_data_c* test_page;
    
    int max_runs = 40;

    //fill cache&db
    cout<<"creating "<<max_runs<<" pages"<<endl;
    for(int i = 0; i < max_runs; ++i) {
        //generate test page url
        std::stringstream ss;
        ss<<"http://test_url_"<<i<<".com/?test_page"<<i<<".html";
        std::string test_url = ss.str();

        //allocate page
        test_page = test_mgr.get_page(test_url);

        //fill with data
        test_page->rank = i;
        ss.str("");
        ss<<"test_cache.cpp generated page "<<i;
        test_page->description = ss.str();

        //meta data
        if(i%2)
            test_page->meta = {"some", "shared", "meta", "but", "with", "unique", "values", "too"};
        else
            test_page->meta = {"some", "shared", "meta", "however", "not", "just"};

        //send back to cache
        test_mgr.put_page(test_page, test_url);
    }
    
    cout<<"done, reading back pages.."<<endl;
    for(int i = 0; i < max_runs; ++i) {
        //generate test page url
        std::stringstream ss;
        ss<<"http://test_url_"<<i<<".com/?test_page"<<i<<".html";
        std::string test_url = ss.str();

        //retrieve page
        test_page = test_mgr.get_page(test_url);

        //display data
        cout<<"page rank: "<<test_page->rank<<endl;
        cout<<"meta: "<<endl;
        for(auto& x: test_page->meta)
            cout<<"\t"<<x<<endl;

        cout<<"description: "<<test_page->description<<endl;

        //free memory - this page will not be put back to memory_mgr for deletion
        //to preserve the ending memory structure after the generating stage.
        //
        //real applications should put pages back..
        test_mgr.put_page(test_page, test_url);
        cout<<"~~~"<<endl;
    }

    cout<<"done!"<<endl;
}
