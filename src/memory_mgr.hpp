#if !defined(MEMORY_MGR_H)
#define MEMORY_MGR_H

#include <iostream>
#include <stdexcept>

#include "page_data.hpp"
#include "cache.hpp"
#include "file_db.hpp"

class robots_txt;

/**
 * generic exception interface to memory_mgr
 *
 * TODO: create a universal crawler_exception?
 */
struct memory_exception: std::exception {
    std::string message;
    const char* what() const noexcept
    {
        return message.c_str();
    }
    memory_exception(std::string s): message(s) {};
};

/**
 * configuration parameters for cache, database and @T object instantiation
 */
struct mmgr_config
{
    std::string database_path;
    std::string object_table;
    std::string user_agent; //when instantiating robots_txt classes
};

/**
 * controls allocating/deletion of page_data_c'
 *      for now only uses new/delete
 *
 * to do/
 *      use a pool of pre allocated page_data_c
 *          -- fifo?
 *      get/put to from pool
 */
template<class T> class memory_mgr
{
    public:
    memory_mgr(struct mmgr_config& config);
    ~memory_mgr(void);

    /**
     * NON-BLOCKING API
     *
     * Methods will throw memory_exception if @T object is locked. Caller
     * should defer work to process later.
     */
    /**
     * Attempts to perform a cache lookup for object @T based on @url hash.
     * If object is found in cache, database is still checked for @url entries
     * and a hash of @T is compared to ensure cache consistancy.
     *
     * If no object is found in cache or database, a new @T is allocated
     *
     * Will throw an exception if object is locked.
     */
    T* get_object_nblk(std::string& url) throw(std::exception);

    /**
     * Tries to store object @T in cache and database at @url key. Object
     * will always be stored to database but may not be cached. If object
     * does not make it to cache it will still be put into the database but
     * is then deleted. As such, @T should not be accessed after calling
     * this method.
     *
     * Will throw an exception if object is locked (should not happen as
     * get_object() performs lock) or connection to database has failed.
     * Caller should re-try later.
     */
    void put_object_nblk(T* t, std::string& url) throw(std::exception);

    /**
     * Attempts to delete object from cache and database.
     *
     * Will throw memory_exception if object is locked or connection to database
     * failed.
     *
     * to do/
     *      mark locked objects as pending delete.
     *      worker thread to delete later
     */
    void delete_object_nblk(T* t, std::string& url) throw(std::exception);

    /**
     * BLOCKING API
     */
    /**
     * NOT IMPLEMENTED.
     */
    T* get_object_blk(std::string& url);

    /**
     * NOT IMPLEMENTED.
     */
    void put_object_blk(T* t, std::string &url);

    /**
     * NOT IMPLEMENTED.
     */
    void delete_object_blk(T* t, std::string& url);

    private:
    struct mmgr_config cfg;

    cache<T>* mem_cache;
    database<T>* mem_db;
};

template<class T> memory_mgr<T>::memory_mgr(struct mmgr_config& config)
{
    cfg = config;
    mem_cache = new cache<T>;
    mem_db = new database<T>(cfg.database_path, cfg.object_table);
}

template<class T> memory_mgr<T>::~memory_mgr(void)
{
    delete mem_cache;
    delete mem_db;
}

template<class T> T* memory_mgr<T>::get_object_nblk(std::string& url) throw(std::exception)
{
    T* t;

    if(!mem_cache->get_object(&t, url)) {
        //objects not in cache need to be allocated first,before haniding
        //to database to fill out.
        t = new T;

        mem_db->get_object(t, url);
    } else {
        //cache coherencey
        if(!mem_db->is_recent(t, url))
            mem_db->get_object(t, url);
    }

    // check if object is locked
    if(t->is_locked())
        throw memory_exception("LOCKED. Cannot access object "+url);
    t->lock();

    return t;
}

template<class T> void memory_mgr<T>::put_object_nblk(T* t, std::string& url) throw(std::exception)
{
    if(!t->is_locked())
        throw memory_exception("UNLOCKED. memory_mgr::put_object_nblk given an unlocked object. Was it allocated by us?");

    mem_db->put_object(t, url);
    if(!mem_cache->put_object(t, url))
        delete t; //object did not make it to cache
    else
        t->unlock();
}

template<class T> void memory_mgr<T>::delete_object_nblk(T* t, std::string& url) throw(std::exception)
{
    if(!t->is_locked)
        throw memory_exception("UNLOCKED. Cannot delete object "+url+" - Was it allocated by us?");

    mem_db->delete_object(url);
    mem_cache->delete_object(url);
    delete t;
}
#endif
