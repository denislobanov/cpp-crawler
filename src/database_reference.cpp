#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "file_db.hpp"

//Local defines
#define DEBUG 2

#if defined(DEBUG)
    #define dbg std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #if DEBUG > 1
        #define dbg_1 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_1 0 && std::cout
    #endif
#else
    #define dbg 0 && std::cout
    #define dbg_1 0 && std::cout
#endif

enum object_state {
    OBJ_UNLOCKED,
    OBJ_LOCKED,
    OBJ_DELETE_PENDING
};

template<class T> database<T>::database(std::string uri, std::string table)
{
    db_path = uri;
    db_table = table;
}

template<class T> database<T>::~database(void) {}

template<class T> void database<T>::put_object(T* t, std::string& key)
{
    //serealize object
    std::ostringstream oss;
    boost::archive::binary_oarchive arch(oss);
    arch<<t;

    //generate filename from key
    std::hash<std::string> h;
    std::stringstream ss;
    ss<<h(key);

    std::string filename = ss.str();

    //write
    file_io_lock.lock();
    std::ofstream file_data(db_path+db_table+filename);

    file_data<<oss.str();
    file_data<<std::endl;
    file_data<<OBJ_UNLOCKED;

    file_data.close();
    file_io_lock.unlock();
}

template<class T> void database<T>::get_object(T* t, std::string& key) throw(std::exception)
{
    
}

template<class T> void database<T>::delete_object(std::string& key) throw(std::exception)
{
}

template<class T> bool database<T>::is_recent(T* t, std::string& key) throw(std::exception)
{
    return true;
}


#if 0

void database::get_page_data(page_data_c* page_data, std::string& url)
{
    std::hash<std::string> url_hash;
    std::stringstream ss;
    std::string filename;

    //create filename for lookup
    ss<<url_hash(url);
    filename = ss.str();

    dbg<<"page_data_c filename: "<<filename<<std::endl;

    //perform lookup
    file_io_lock.lock();
    std::ifstream file_stream(db_path+"/page_data/"+filename);
    file_io_lock.unlock();

    if(file_stream) {
        dbg<<"stream valid"<<std::endl;
        std::string line;
        while(file_stream.good()) {
            //first line of record is the page rank
            getline(file_stream, line);
            ss.str(line);
            ss >> page_data->rank;
            dbg_1<<"rank line: "<<line<<std::endl;

            //crawl count
            getline(file_stream, line); //blank line field seperator
            getline(file_stream, line);
            ss.clear(); //FIXME -- this should /not/ be needed (see get_robots_txt)
            ss.str(line);
            ss >> page_data->crawl_count;
            dbg_1<<"crawl count line: "<<line<<std::endl;

            //last crawl
            getline(file_stream, line);
            getline(file_stream, line);
            ss.str(line);
            std::time_t tmp_t;
            ss >> tmp_t;
            page_data->last_crawl = std::chrono::system_clock::from_time_t(tmp_t);
            dbg_1<<"last crawl line: "<<line<<std::endl;

            //page title
            getline(file_stream, line);
            getline(file_stream, line);
            page_data->title += unescape_string(line);
            dbg_1<<"page title line ["<<line<<"]\n";

            //description
            getline(file_stream, line);
            getline(file_stream, line);     //first line of description
            while(!line.empty()) {          //rest of description
                dbg_1<<"description line ["<<line<<"]\n";
                page_data->description += unescape_string(line);
                page_data->description += "\n";
                getline(file_stream, line);
            }

            //meta data
            getline(file_stream, line);     //last getline consumes field seperator
            while(!line.empty()) {
                page_data->meta.push_back(unescape_string(line));
                getline(file_stream, line);
            }

            //out links
            getline(file_stream, line);
            while(!line.empty()) {
                page_data->out_links.push_back(unescape_string(line));
                getline(file_stream, line);
            }
        }

        //perform some sort of file/access lock here (db side)
    }

    file_stream.close();
}

void database::store_keywords(page_data_c* page_data, std::string page_hash)
{
    for(auto& keyword: page_data->meta) {
        dbg<<"keyword: "<<escape_string(keyword)<<std::endl;

        //each keyword has its own "meta" file, which contains hashes of matching pages
        file_io_lock.lock();
        //first we read to see if keyword is already associated with page
        std::fstream meta_file(db_path+"/meta_data/"+escape_string(keyword), std::fstream::in);
        file_io_lock.unlock();

        //check if page-keyword association already exists.
        bool meta_present = false;

        if(meta_file) {
            while(meta_file.good()) {
                std::string line;
                getline(meta_file, line);

                //already in (keyword)meta_file
                if(is_inst(line, page_hash)) {
                    meta_present = true;
                    break;
                }
            }
        }
        meta_file.close();

        //new keyword and/or file
        if(!meta_present) {
            //open for writing
            file_io_lock.lock();
            meta_file.open(db_path+"/meta_data/"+escape_string(keyword), std::fstream::out|std::fstream::app);
            file_io_lock.unlock();

            meta_file<<page_hash<<std::endl;

            meta_file.flush();
            meta_file.close();
        }
    }
}

void database::put_page_data(page_data_c* page_data, std::string& url)
{
    std::hash<std::string> url_hash;
    std::stringstream ss;
    std::string filename;

    //create filename
    ss<<url_hash(url);
    filename = ss.str();

    //avoid concurrent writes to the same file
    file_io_lock.lock();
    std::ofstream file_data(db_path+"/page_data/"+filename);
    file_io_lock.unlock();

    //appending data is the responsibility of the crawler proccess
    if(file_data) {
        //rank
        file_data<<page_data->rank<<std::endl;
        file_data<<std::endl; //blank line seperates field data

        //crawl count
        file_data<<page_data->crawl_count<<std::endl;
        file_data<<std::endl;

        //last crawl
        std::time_t tmp_t = std::chrono::system_clock::to_time_t(page_data->last_crawl);
        file_data<<tmp_t<<std::endl;
        file_data<<std::endl;

        //title
        file_data<<escape_string(page_data->title)<<std::endl;
        file_data<<std::endl;

        //description
        file_data<<escape_string(page_data->description)<<std::endl;
        file_data<<std::endl;

        //meta - this avoid searching meta_data dir on get_page_data
        for(auto& i: page_data->meta)
            file_data<<escape_string(i)<<std::endl;
        file_data<<std::endl;

        //out links
        for(auto& i: page_data->out_links)
            file_data<<escape_string(i)<<std::endl;
    }

    file_data.close();

    //update keywords library
    store_keywords(page_data, filename);
}

void database::get_robots_txt(robots_txt* robots, std::string& url)
{
    std::hash<std::string> url_hash;
    std::stringstream ss;
    std::string filename;

    //create filename for lookup
    ss<<url_hash(url);
    filename = ss.str();

    dbg<<"robots_txt filename: "<<filename<<std::endl;

    //perform lookup
    file_io_lock.lock();
    std::ifstream file_stream(db_path+"/robots/"+filename);
    file_io_lock.unlock();

    if(file_stream) {
        dbg<<"stream valid"<<std::endl;
        std::string line;
        while(file_stream.good()) {
            //first line of record is last_visit
            getline(file_stream, line);
            ss.str(line);
            ss >> robots->last_visit;

            //crawl_delay
            getline(file_stream, line); //blank line seperates field data
            getline(file_stream, line);
            ss.str(line);
            ss >> robots->crawl_delay;

            //robots txt exclusion list
            getline(file_stream, line);
            getline(file_stream, line);
            if(!line.empty()) {
                std::vector<std::string> exclusions;

                while(!line.empty()) {
                    exclusions.push_back(line);
                    getline(file_stream, line);
                }
                robots->import_exclusions(exclusions);
            }

            //robots txt inclusion list
            getline(file_stream, line);
            if(!line.empty()) {
                std::vector<std::string> inclusions;

                while(!line.empty()) {
                    inclusions.push_back(line);
                    getline(file_stream, line);
                }
                robots->import_inclusions(inclusions);
            }
        }

        //perform some sort of file/access lock here (db side)
    }

    file_stream.close();
}


void database::put_robots_txt(robots_txt* robots, std::string& url)
{
    std::hash<std::string> url_hash;
    std::stringstream ss;
    std::string filename;

    std::vector<std::string> exclusions;
    std::vector<std::string> inclusions;

    //create filename
    ss<<url_hash(url);
    filename = ss.str();

    //avoid concurrent writes to the same file
    file_io_lock.lock();
    std::ofstream file_data(db_path+"/robots/"+filename);
    file_io_lock.unlock();

    //appending data is the responsibility of the crawler proccess
    if(file_data) {
        //last visit
        file_data<<robots->last_visit<<std::endl;
        file_data<<std::endl;

        //crawl delay
        file_data<<robots->crawl_delay<<std::endl;
        file_data<<std::endl;

        //both inclusions and exclusions vectors are independantly optional
        if(robots->export_exclusions(exclusions)) {
            dbg_1<<"exclusion list present, saving\n";
            for(auto& ex_line : exclusions)
                file_data<<ex_line<<std::endl;
        }

        if(robots->export_inclusions(inclusions)) {
            dbg_1<<"niclusion list present, saving\n";
            file_data<<std::endl;
            for(auto& inc_line: inclusions)
                file_data<<inc_line<<std::endl;
        }
    }

    file_data.close();
}

void database::rm_page_data(page_data_c* page_data, std::string& url)
{
    std::hash<std::string> url_hash;
    std::stringstream ss;
    std::string filename;

    //create filename
    ss<<url_hash(url);
    filename = db_path+"/page_data/"+ss.str();
    dbg<<"deleting page ["<<filename<<" ["<<url<<"]]\n";

    //avoid concurrent writes to the same file
    file_io_lock.lock();
    int ret = remove(filename.c_str());
    file_io_lock.unlock();

    if(!ret)
        dbg<<"failed to delete page ["<<filename<<"]\n";
}

bool database::page_in_sync(page_data_c* page_data, std::string& url)
{
    dbg<<"FIXME: database::page_in_sync not implemented\n";
    return true;
}
#endif
