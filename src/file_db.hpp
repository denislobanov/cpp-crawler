#if !defined(DATABASE_H)
#define DATABASE_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <mutex>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

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
enum object_state {
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
    ~database(void) {}

    /**
     * Blocking synchronous call to retrieve object from database. Automatically
     * locks entry to prevent concurrent access.
     *
     * On success @t is returned from database. If no database entry exists
     * for @key, @t is unmodified. Will throw exception if @key is pending delete.
     * Calling process should discard trying to process this data.
     */
    void get_object(T& t, std::string& key) throw(std::exception)
    {
        //generate db query
        std::hash<std::string> h;
        std::stringstream ss;
        ss<<h(key);

        std::string filename = ss.str();

        //read from database
        file_io_lock.lock();
        std::ifstream file_data(db_path+"/"+db_table+"/"+filename);
        if(file_data) {
            //deserealize
            boost::archive::binary_iarchive arch(file_data);
            arch>> t;

            file_data.close();
        }
        //if file does not exist we simply leave @t as is

        file_io_lock.unlock();
    }

    /**
     * Blocking call to put object to database. If object was previously
     * retrieved by this session its entry is unlocked afterwards. Concurrent
     * writes to an unlocked object are expected to be handled by the database
     * implementation.
     */
    void put_object(T& t, std::string& key)
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
        std::ofstream file_data(db_path+"/"+db_table+"/"+filename);

        file_data<<oss.str();
        file_data<<std::endl;
        file_data<<OBJ_UNLOCKED;

        file_data.close();
        file_io_lock.unlock();
    }

    /**
     * Sets object state to 'OBJ_DELETE_PENDING' to prevent get_object deadlocks
     * in concurrent processes before deleting object.
     *
     * Throws exception if connection/database error occured.
     */
    void delete_object(std::string& key) throw(std::exception)
    {
        //generate db query
        std::hash<std::string> h;
        std::stringstream ss;
        ss<<h(key);

        std::string filename = db_path+"/"+db_table+"/"+ss.str();

        //delete object
        file_io_lock.lock();
        int r = remove(filename.c_str());
        file_io_lock.unlock();

        if(!r)
            throw db_exception("failed to delete file: "+filename);
    }

    /**
     * Checks if the data in memory is in sync with that in the database
     * returns true if it is.
     *
     * As this function requires a read lock, it will throw an exception
     * if database object state is 'OBJ_PENDING_DELETE'.
     */
    bool is_recent(T& t, std::string& key) throw(std::exception)
    {
        return true;
    }

    private:
    std::mutex file_io_lock;    //concurrent open to the same file
    std::string db_path;
    std::string db_table;
};

#endif
