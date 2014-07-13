#include <iostream>
#include <chrono>
#include <boost/filesystem.hpp>

#include "page_data.hpp"

using std::cout;
using std::endl;

void print_usage(void)
{
    cout<<"Usage:\n\tlist_pages <path to table within file database>"<<endl;
    cout<<"About:\n\tConverts the collection of hash files in a file db table into a usable format; listing a digested view of each page consisting of the page url, last access time and crawl count"<<endl;
    cout<<"Example:\n\tlist_pages ./test_db/page_table"<<endl;
}

int main(int argc, char* argv[])
{
    if(argc != 2) {
        print_usage();
        return 1;
    }

    path p(argv[1]);
    if(exists(p)) {
        if(is_directory(p)) {
            copy(directory_iterator(p), directory_iterator(),
                ostream_iterator<directory_iterator>()
    int i;
    for(i=1;i<argc;++i) {
        cout<<"Reading page "<<argv[i]<<endl;
        cout<<"-------------"<<endl;

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
            cout<<"\n------------"<<endl;
        }
    }
    return 0;
}
