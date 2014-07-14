#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <boost/archive/binary_iarchive.hpp>

#include "page_data.hpp"

using std::cout;
using std::endl;

void print_usage(void)
{
    cout<<"Usage:\n\tdump_page [list of pages]"<<endl;
    cout<<"About:\n\tDumps the entire contents of [n] pages_data_c entries from a file_db type database."<<endl;
    cout<<"Example:\n\tdump_page 6979709447096664111 4990958972598834934"<<endl;
}

int main(int argc, char* argv[])
{
    if(argc < 2) {
        print_usage();
        return 1;
    }

    int i;
    for(i=1;i<argc;++i) {
        cout<<"Reading page "<<argv[i]<<endl;
        cout<<"---"<<endl;

        //iterate through all given pages. try to retrieve and dump each one
        std::ifstream file_data(argv[i]);
        if(file_data) {
            page_data_c page;
            //deserealize
            boost::archive::binary_iarchive arch(file_data);
            arch>>page;
            file_data.close();

            cout<<"url: ["<<page.url<<"]"<<endl;
            cout<<"rank: ["<<page.rank<<"]"<<endl;
            cout<<"crawl count: ["<<page.crawl_count<<"]"<<endl;
            cout<<"last crawl: ["<<std::chrono::system_clock::to_time_t(page.last_crawl)<<"]"<<endl;
            cout<<"title: ["<<page.title<<"]"<<endl;
            cout<<"description: ["<<page.description<<"]"<<endl;
            cout<<"out links:"<<endl;
            for(auto& x: page.out_links)
                cout<<"\t"<<x<<endl;
            cout<<"\nmeta:"<<endl;
            for(auto& x: page.meta)
                cout<<"\t"<<x<<endl;
            cout<<"\n----"<<endl;
        }
    }
    return 0;
}
