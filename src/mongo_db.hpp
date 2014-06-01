#if !defined(DATABASE_H)
#define DATABASE_H

#include <iostream>
#include <functional>

#include "mongo/client/dbclient.h"

#include "page_data.hpp"
#include "robots_txt.hpp"

/**
 * generic exception interface to database client
 */
struct db_exception: std::exception {
    std::string message;
    const char* what() const noexcept
    {
        return message.c_str();
    }
    db_exception(std::string s): message(s) {};
};

/**
 * enum used by class for internal state tracking (locking)
 */
static enum object_state {
    OBJ_UNLOCKED,
    OBJ_LOCKED,
    OBJ_DELETE_PENDING
};

template<typename T> class database
{
    public:
    /**
     * On creation, object connects to the database at @uri. All object access
     * will occure from @table
     */
    database(std::string uri, std::string table)
    {
        db_path = uri;
        db_table = table;
    }

    /**
     * Blocking synchronous call to retrieve object from database. Automatically
     * locks entry to prevent concurrent access.
     *
     * On success @t is returned from database. If no database entry exists
     * for @key, @t is unmodified. Will throw exception if @key is pending delete.
     * Calling process should discard trying to process this data.
     */
    void get_object(T& t, std::string& key) throw(std::exception);

    /**
     * Blocking call to put object to database. If object was previously
     * retrieved by this session its entry is unlocked afterwards. Concurrent
     * writes to an unlocked object are expected to be handled by the database
     * implementation.
     */
    void put_object(T& t, std::string& key);

    /**
     * Sets object state to 'OBJ_DELETE_PENDING' to prevent get_object deadlocks
     * in concurrent processes before deleting object.
     *
     * Throws exception if connection/database error occured.
     */
    void delete_object(std::string& key) throw(std::exception);

    /**
     * Checks if the data in memory is in sync with that in the database
     * returns true if it is.
     *
     * As this function requires a read lock, it will throw an exception
     * if database object state is 'OBJ_PENDING_DELETE'.
     */
    bool is_recent(T& t, std::string& key) throw(std::exception);

    private:
    std::string db_path;
    std::string db_table;
};

#endif
