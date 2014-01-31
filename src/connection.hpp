#if !defined(CONNECTION_H)
#define CONNECTION_H

#include <iostream>
#include <vector>
#include <iomanip>
#include <string>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>

#include "ipc_common.hpp"

class connection
{
    public:
    connection(boost::asio::io_service& io_service): socket_(io_service)
    {
        std::string s;
        std::ostringstream oss;
        boost::archive::binary_oarchive arch(oss);
        arch<<header;

        s = oss.str();
        header_raw_size = s.size();
        rx_header.resize(header_raw_size);
    }

    boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    //set data type prior to transmittion
    void wdata_type(data_type_e t)
    {
        header.data_type  = t;
    }

    //get transmitted data type
    data_type_e rdata_type(void)
    {
        return header.data_type;
    }

    //serealize data prior to transmittion
    template<typename T> void wdata(T t)
    {
        std::ostringstream oss;
        boost::archive::binary_oarchive arch(oss);
        arch<<t;

        tx_data = oss.str();
        header.data_size = tx_data.size();
    }

    //de-serealized data from transmittion
    template<typename T> T rdata(void)
    {
        T t;
        std::string archive_data(&rx_data[0], rx_data.size());
        std::istringstream iss(archive_data);
        boost::archive::binary_iarchive arch(iss);

        arch>>t;
        return t;
    }

    //transmit serealized data
    template<typename Handler> void async_write(Handler handler)
    {
        //data already serialized by wdata()
        std::ostringstream oss;
        boost::archive::binary_oarchive arch(oss);
        arch<<header;
        tx_header = oss.str();

        if(!oss || tx_header.size() != header_raw_size) {
            //send error to handler
            std::cerr<<"async_write boundry error. tx_header.size(): "<<tx_header.size()<<" != header_raw_size "<<header_raw_size<<"\n";
            boost::system::error_code err(boost::asio::error::invalid_argument);
            socket_.get_io_service().post(boost::bind(handler, err));
            return;
        }

        //write to socket
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(tx_header));
        buffers.push_back(boost::asio::buffer(tx_data));

        boost::asio::async_write(socket_, buffers, handler);
    }

    template<typename Handler> void async_read(Handler handler)
    {
        //first read the header from socket
        void (connection::*f)(const boost::system::error_code&, boost::tuple<Handler>)
            = &connection::read_header<Handler>;

        boost::asio::async_read(socket_, boost::asio::buffer(rx_header),
            boost::bind(f, this, boost::asio::placeholders::error,
                boost::make_tuple(handler)));
    }

    template<typename Handler>
    void read_header(const boost::system::error_code& ec, boost::tuple<Handler> handler)
    {
        if(!ec) {
            //get&deserealize header
            try {
                std::istringstream iss(std::string(&rx_header[0], header_raw_size));
                boost::archive::binary_iarchive arch(iss);

                arch>>header;
            } catch (std::exception& e) {
                std::cerr<<"read_header cought exception: "<<e.what()<<std::endl;
                boost::system::error_code err(boost::asio::error::invalid_argument);
                boost::get<0>(handler)(err);
                return;
            }

            if(!header.data_size) {
                std::cout<<"!header.data_size\n";
                boost::system::error_code err(boost::asio::error::invalid_argument);
                boost::get<0>(handler)(err);
                return;
            }

            //now async_read data
            rx_data.resize(header.data_size);

            //read data from socket, call handler on completion - caller
            //must explicitly deserealise data
            void (connection::*f)(const boost::system::error_code&, boost::tuple<Handler>)
                = &connection::read_data<Handler>;

            boost::asio::async_read(socket_, boost::asio::buffer(rx_data),
                boost::bind(f, this, boost::asio::placeholders::error,
                    handler));
        } else {
            //call handler with error code
            boost::get<0>(handler)(ec);
        }
    }

    template<typename Handler>
    void read_data(const boost::system::error_code& ec, boost::tuple<Handler> handler)
    {
        boost::get<0>(handler)(ec);
    }

    private:
    boost::asio::ip::tcp::socket socket_;

    struct header_s {
        data_type_e data_type;
        std::size_t data_size;

        template<class Archive>
        void serialize(Archive& ar, const unsigned int version)
        {
            ar & data_type;
            ar & data_size;
        }
    } header;

    //half duplex communication, though full duplex can easily be implemented
    //by not sharing the header above with tx&rx functions
    std::string tx_data;
    std::string tx_header;
    std::vector<char> rx_data;
    std::vector<char> rx_header;

    std::size_t header_raw_size;
};

#endif
