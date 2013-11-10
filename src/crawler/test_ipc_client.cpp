#include <iostream>
#include <sstream>

#include "ipc_client.hpp"

using std::cout;
using std::endl;

int main(void)
{
    ipc_client test_ipc;

    //local control
    const int loops = 10;

    int i;
    cout<<"begin test"<<endl;
    for(i = 0; i < loops; ++i) {
        queue_node_s data;
        std::stringstream ss;

        data.credit = i;
        ss<<"http://test_url_"<<i<<".com/?test_page"<<i<<".html";
        data.url = ss.str();
        
        cout<<"adding item "<<i<<"to queue\n\tcredit: "<<data.credit<<"\n\ttest_url ["<<data.url<<"]"<<endl;
        test_ipc.send_item(data);
    }

    cout<<"---\nretireiving data.."<<endl;
    for(i=0; i< loops; ++i) {
        queue_node_s data = test_ipc.get_item();
        cout<<"item "<<i<<"on queue\n\tcredit: "<<data.credit<<"\n\ttest_url ["<<data.url<<"]"<<endl;
    }

    return 0;
}
