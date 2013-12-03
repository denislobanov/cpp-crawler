#include <iostream>
#include <fstream>
#include <streambuf>
#include <vector>

#include "html_normalise.hpp"

using std::cout;
using std::endl;

int main(void)
{
    cout<<"loading test page"<<endl;
    std::ifstream file("test_file2.html");
    std::string web_page;

    file.seekg(0, std::ios::end);
    web_page.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    web_page.assign((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    //html tags to look for
    cout<<"init html tidy"<<endl;
    html_normalise test_normaliser;

    cout<<"normalise data\n";
    int i = test_normaliser.normalise(web_page);
    cout<<"normaliser returned "<<i<<endl;
    return 0;
}
