#include <iostream>
#include <chrono>

#include "robots_txt.hpp"

using std::cout;
using std::endl;

void print_usage(void)
{
    cout<<"Usage:\n\tdump_robots [list of robots_txt entries]"<<endl;
    cout<<"About:\n\tDumps the entire contents of [n] robots_txt object entries from a file_db type database."<<endl;
    cout<<"Example:\n\tdump_robots ./test_db/robots_table/16048585540657447894"<<endl;
}

int main(int argc, char* argv[])
{
    if(argc < 2) {
        print_usage();
        return 1;
    }

    int i;
    for(i=1;i<argc;++i) {
        cout<<"Reading robots_txt "<<argv[i]<<endl;
        cout<<"------------------"<<endl;

        //iterate through all given pages. try to retrieve and dump each one
        std::ifstream file_data(argv[i]);
        if(file_data) {
            robots_txt robot;
            //deserealize
            boost::archive::binary_iarchive arch(file_data);
            arch>>robot;
            file_data.close();

            cout<<"last visit: ["<<std::chrono::system_clock::to_time_t(write_robots->last_visit())<<"]"<<endl;
            cout<<"crawl delay: ["<<write_robots->crawl_delay().count()<<"]"<<endl;
            cout<<"\n------------------"<<endl;
        }
    }
    return 0;
}
