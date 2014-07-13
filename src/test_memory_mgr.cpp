#include <iostream>
#include <vector>
#include <sstream>
#include <ctime>
#include <chrono>

#include "page_data.hpp"
#include "memory_mgr.hpp"

using std::cout;
using std::endl;

int main(void)
{
    //memory_mgr configuration
    mmgr_config config = {
        .database_path = "./test_db/",
        .object_table = "page_table",
        .user_agent = "test_mem_mgr"
    };

    memory_mgr<page_data_c> test_mgr(config);
    page_data_c* test_page;
    
    int max_runs = CACHE_MAX*2;

    //fill cache&db
    cout<<"creating "<<max_runs<<" pages"<<endl;
    for(int i = 0; i < max_runs; ++i) {
        //generate test page url
        std::stringstream ss;
        ss<<"http://test_url_"<<i<<".com/?test_page"<<i<<".html";
        std::string test_url = ss.str();

        //allocate page
        test_page = test_mgr.get_object_nblk(test_url);

        //fill with data
        test_page->rank = i;
        ss.str("");
        ss<<"test_cache.cpp generated page "<<i;
        test_page->description = ss.str();
        ss.str("");
        ss<<"title "<<i<<"!";
        test_page->title = ss.str();
        test_page->last_crawl = std::chrono::system_clock::now();

        //meta data
        if(i%2)
            test_page->meta = {"some", "shared", "meta", "but", "with", "unique", "values", "too"};
        else
            test_page->meta = {"some", "shared", "meta", "however", "not", "just"};

        //send back to cache
        test_mgr.put_object_nblk(test_page, test_url);
    }
    
    cout<<"done, reading back pages.."<<endl;
    for(int i = 0; i < max_runs; ++i) {
        //generate test page url
        std::stringstream ss;
        ss<<"http://test_url_"<<i<<".com/?test_page"<<i<<".html";
        std::string test_url = ss.str();

        //retrieve page
        test_page = test_mgr.get_object_nblk(test_url);

        //display data
        cout<<"page rank: "<<test_page->rank<<endl;

        std::time_t last_c = std::chrono::system_clock::to_time_t(test_page->last_crawl);
        cout<<"last crawl: "<<std::ctime(&last_c)<<endl;
        cout<<"pag titile: "<<test_page->title<<endl;
        cout<<"meta: "<<endl;
        for(auto& x: test_page->meta)
            cout<<"\t"<<x<<endl;

        cout<<"description: "<<test_page->description<<endl;

        //free memory - this page will not be put back to memory_mgr for deletion
        //to preserve the ending memory structure after the generating stage.
        //
        //real applications should put pages back..
        test_mgr.put_object_nblk(test_page, test_url);
        cout<<"~~~"<<endl;
    }

    cout<<"done!"<<endl;
}
