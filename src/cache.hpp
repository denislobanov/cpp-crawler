#if !defined(CACHE_H)
#define CACHE_H

#include <iostream>
#include <map>
#include <unordered_map>
#include <mutex>
#include <chrono>

#include "page_data.hpp"
#include "robots_txt.hpp"
#include "debug.hpp"

//total number of objects stored in cache
#define CACHE_MAX   10
//TO DO: number of entries to keep free
#define CACHE_RES   0
//total cache size is thus CACHE_MAX - CACHE_RES

template<class T> class cache
{
    public:
    cache(void);

    /**
     * Blocking synchronous call to retrieve object from cache. Automatically
     * locks entry to prevent concurrent access.
     *
     * If @key entry exists in cache, @t* will point to valid memory and
     * method returns true. If no entry exists method returns false, @t*
     * is undefined and should not be used.
     */
    bool get_object(T** t, std::string& key);

    /**
     * Blocking call to put object into cache. If object was previously
     * retrieved by this session its entry is unlocked afterwards. Concurrent
     * writes should not occure and method behaviour is then undefined.
     *
     * Will return true if @t made it to cache.
     */
    bool put_object(T* t, std::string& key);

    /**
     * Blocking method to remove an entry from cache. This will not delete
     * allocated memory and leaves it locked to prevent concurrent access
     * to objects about to be deleted.
     */
    void delete_object(std::string& key);

    private:
    typedef std::chrono::steady_clock::time_point time_point_t;
    typedef std::map<time_point_t, std::string> access_map_t;

    struct cache_entry_s {
        T* t;
        time_point_t timestamp;
        // semaphore access counter
    };
    typedef std::unordered_map<std::string, struct cache_entry_s> data_map_t;

    data_map_t obj_map;
    access_map_t obj_access;
    std::mutex rw_mutex;
    int fill;

    //non-threaded
    void prune_cache(void);
    void update_timestamp(std::string& key, time_point_t new_time);
};

template<class T> cache<T>::cache(void)
{
    fill = 0;
}

template<class T> void cache<T>::update_timestamp(std::string& key, time_point_t new_time)
{
    //delete old timestamp from access map
    time_point_t old_time = obj_map.at(key).timestamp;
    obj_access.erase(obj_access.find(old_time));

    //update data timestamp
    obj_map.at(key).timestamp = new_time;

    //update access time
    obj_access.insert(std::pair<time_point_t, std::string>(new_time, key));
}

template<class T> bool cache<T>::get_object(T** t, std::string& key)
{
    bool in_cache = false;

    rw_mutex.lock();
    try {
        //will throw exception if page is not in cache
        *t = obj_map.at(key).t;
        update_timestamp(key, std::chrono::steady_clock::now());
        in_cache = true;

        dbg<<"object ["<<key<<"] in cache\n";
    } catch(const std::out_of_range& e) {
        dbg_1<<"object ["<<key<<"] not in cache\n";
    }
    rw_mutex.unlock();

    return in_cache;
}

template<class T> bool cache<T>::put_object(T* t, std::string& key)
{
    time_point_t new_time = std::chrono::steady_clock::now();

    rw_mutex.lock();
    try {
        //updating timestamp accesses object in cache (we will need to update
        //timestamp of existing objects anyway). throws exception for new objects
        update_timestamp(key, new_time);
        dbg<<"object ["<<key<<"] already in cache, updating\n";
    } catch(const std::out_of_range& e) {
        access_map_t::iterator pos = obj_access.end();
        struct cache_entry_s entry;
        entry.t = t;
        entry.timestamp = new_time;

        if(fill < CACHE_MAX) {
            //when there's room to fill cache, fill!
            dbg<<"space in cache to insert object ["<<key<<"]\n";
            obj_map.insert(std::pair<std::string, struct cache_entry_s>(key, entry));

            dbg_1<<"updating access map\n";
            obj_access.insert(pos, std::pair<time_point_t, std::string>(new_time, key));

            ++fill;

            //usually un-needed, but a number of entries in a mature cache
            //could be deleted.
            prune_cache();
        } else {
            std::string oldest_key = obj_access.begin()->second;

            //we do not know the object type, and whilst page objects have
            //a rank field, robots do not.
            dbg<<"replacing ["<<oldest_key<<"] in cache for ["<<key<<"]\n";
            obj_access.erase(obj_map.at(oldest_key).timestamp);
            obj_map.erase(obj_map.find(oldest_key));

            obj_map.insert(std::pair<std::string, struct cache_entry_s>(key, entry));
            obj_access.insert(std::pair<time_point_t, std::string>(new_time, key));
        }
    }
    rw_mutex.unlock();

    dbg<<"done\n";
    return true;
}

/**
 * Helper function to remove excess (>CACHE_MAX) entries from cache and
 * cache maps
 */
template<class T> void cache<T>::prune_cache(void)
{
    while(fill-(CACHE_MAX-CACHE_RES) > 0) {
        dbg_1<<"objects to prune "<<(fill-(CACHE_MAX-CACHE_RES))<<std::endl;

        delete obj_map.at(obj_access.begin()->second).t;
        obj_map.erase(obj_map.find(obj_access.begin()->second));
        obj_access.erase(obj_access.begin());
        --fill;
    }
}

template<class T> void cache<T>::delete_object(std::string& key)
{
    dbg<<"deleting object at ["<<key<<"]\n";

    rw_mutex.lock();
    try {
        //reverse lookup access map entries (ame)
        time_point_t ame = obj_map.at(key).timestamp;

        dbg<<"removing page ["<<key<<"]\n";
        obj_access.erase(obj_access.find(ame));
        obj_map.erase(key);
    } catch(const std::exception& e) {
        dbg<<"cannot delete page - not in cache\n";
    }
    rw_mutex.unlock();
}

#endif
