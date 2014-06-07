#include <iostream>
#include <fstream>
#include <streambuf>
#include <vector>

#include "parser.hpp"
#include "ipc_common.hpp"

using std::cout;
using std::endl;

int main(void)
{
    cout<<"loading test page"<<endl;
    std::ifstream file("test_file.html");
    std::string web_page;

    file.seekg(0, std::ios::end);
    //~ web_page.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    //~ web_page.assign((std::istreambuf_iterator<char>(file)),
        //~ std::istreambuf_iterator<char>());

    //html tags to look for
    cout<<"creating parser config"<<endl;
    std::vector<struct tagdb_s> parse_param;
    struct tagdb_s param;

    param.tag_type = tag_type_url;
    param.xpath = "//a[@href]";
    param.attr = "href";
    parse_param.push_back(param);

    param.tag_type = tag_type_meta;
    param.xpath = "//p";
    param.attr = "";
    parse_param.push_back(param);

    param.tag_type = tag_type_title;
    param.xpath = "//title";
    param.attr = "";
    parse_param.push_back(param);

    //create parser
    cout<<"initialising parser"<<endl;
    parser test_parser("http://www.xmlsoft.org/");

    cout<<"parsing"<<endl;
    //try catch block here
    test_parser.parse(parse_param);

    //process data
    cout<<"processing data"<<endl;
    int i = 0;
    for(auto& entry: test_parser.data) {
        cout<<"entry "<<i<<endl;
        cout<<"tag name ["<<entry.tag_name<<"]"<<endl;
        cout<<"tag data ["<<entry.tag_data<<"]"<<endl;
        cout<<"tag attr_data ["<<entry.attr_data<<"]"<<endl;
        cout<<endl;
        ++i;
    }

    cout<<"done"<<endl;
    return 0;
}
