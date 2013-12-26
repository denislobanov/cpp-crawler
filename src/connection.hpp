#if !defined(CONNECTION_H)
#define CONNECTION_H

#include <iostream>
#include <vector>
#include <boost/asio.hpp>

#include "ipc_common.hpp"

class connection
{
    public:
    connection(boost::asio::io_service& io_service): socket_(io_service) {}

    boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    //get transmitted data type
    data_type_e data_type(void)
    {
        return data_type_;
    }

    //set data type prior to transmittion
    void data_type(data_type_e t)
    {
        data_type_  = t;
    }

    //de-serealizes raw data from transmittion
    template<typename T> T data(void)
    {
        T t;
        std::string archive_data(&raw_data[0], raw_data.size());
        std::istringstream iss(archive_data);
        boost::archive::binary_iarchive arch(iss);

        arch>>t;
    }

    //serelize data prior to transmittion
    template<typename T> void data(T& t)
    {
        std::ostringstream oss;
        boost::archive::binary_oarchive arch(oss);
        arch<<t;

        raw_data = arch.str();
    }

    //transmit serealized data
    template<typename Handler> void async_write(Handler handler)
    {
        std::ostringstream oss;

        //create fixed size header
        oss<<std::setw(header_length);
        oss<<std::hex<<data_type;
        oss<<std::hex<<raw_data.size();
        raw_header = oss.str();
        
        if(!oss || raw_header.size() != header_length) {
            //send error to handler
            boost::system::error_code err(boost::asio::error::invalid_argument);
            socket_.get_io_service().post(boost::bind(handler, err));
            return;
        }

        //write to socket
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(raw_header));
        buffers.push_back(boost::asio::buffer(raw_data));

        boost::asio::async_write(socket_, buffers, handler);
    }

    template<typename Handler> void async_read(Handler handler)
    {
        //first read the header from socket
        void (connection::*f)(
            const boost::system::error_code&, boost::tuple<Handler>)
        = &connection::read_header<Handler>;

        boost::asio::async_read(socket_, boost::asio::buffer(header),
            boost::bind(f, this, boost::asio::placeholders::error,
                boost::make_tuple(handler)));
    }

    template<typename Handler>
    void read_header(const boost::system::error_code& ec, boost::tuple<Handler> handler)
    {
        if(!ec) {
            std::istringstream iss(std::string(raw_header, header_length));
            std::size_t data_size = 0;

            //get header data
            iss>>std::hex>>data_type;
            iss>>std::hex>>data_size;

            if(!data_size) {
                boost::system::error_code err(boost::asio::error::invalid_argument);
                socket_.get_io_service().post(boost::bind(handler, err));
                return;
            }

            //now async_read data
            raw_data.resize(data_size);

            //read data from socket, call handler on completion - caller
            //must explicitly deserealise data
            boost::asio::async_read(socket_, boost::asio::buffer(data), handler);
        } else {
            //call handler with error code
            boost::get<0>(handler)(ec);
        }
    }

    private:
    boost::asio::ip::tcp::socket socket_;

    //half duplex communication
    std::string raw_data;
    std::string raw_header;

    enum {header_length = 36};
    data_type_e data_type;
};

#endif
