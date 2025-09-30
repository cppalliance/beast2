//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_WORKER_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_WORKER_HPP

#include "asio_server.hpp"
#include "handler.hpp"
#include "logger.hpp"
#include <boost/http_io/read.hpp>
#include <boost/http_io/write.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <stddef.h>
#include <string>

#include <boost/asio/ip/tcp.hpp> // VFALCO REMOVE
#include <functional> // for _1

namespace boost {
namespace http_io {

void
service_unavailable(
    http_proto::request_base const& req,
    http_proto::response& res,
    http_proto::serializer& sr);

template<
    class Executor,
    class Executor1 = Executor>
class worker
{
private:
    // order of destruction matters here
    asio_server& srv_;
    section sect_;
    boost::asio::basic_socket_acceptor<
        boost::asio::ip::tcp, Executor1>* pa_ = nullptr;
    boost::asio::basic_stream_socket<
        boost::asio::ip::tcp, Executor> sock_;
    boost::asio::ip::tcp::endpoint ep_;
    std::string const& doc_root_;
    http_proto::request_parser pr_;
    http_proto::response res_;
    http_proto::serializer sr_;
    std::size_t id_ = 0;

public:
    worker(
        asio_server& srv,
        std::string const& doc_root)
        : srv_(srv)
        , sock_(srv.get_executor())
        , doc_root_(doc_root)
        , pr_(srv.services())
        , sr_(srv.services())
        , id_(srv.make_unique_id())
    {
    }

    /** Run the worker I/O loop

        The worker will continue to run until stop is called
        or the worker is destroyed, whichever comes first.
    */
    void
    run(boost::asio::basic_socket_acceptor<
        boost::asio::ip::tcp, Executor1>& acc)
    {
        pa_ = &acc;
        do_accept();
    }

    void
    stop()
    {
        boost::system::error_code ec;
        sock_.cancel(ec);
    }

private:
    std::string id() const
    {
        return std::string("[") + std::to_string(id_) + "] ";
    }

    void
    fail(
        std::string what,
        boost::system::error_code ec)
    {
        if( ec == asio::error::operation_aborted )
            return;

        if( ec == asio::error::eof )
        {
            sock_.shutdown(
                asio::socket_base::shutdown_send, ec);
            return;
        }

    #ifdef LOGGING
        std::cerr <<
            what << "[" << id_ << "]: " <<
            ec.message() << "\n";
    #endif
    }

    void
    do_accept()
    {
        // Clean up any previous connection.
        boost::system::error_code ec;
        sock_.close(ec);
        pr_.reset();

        using namespace std::placeholders;
        pa_->async_accept( sock_, ep_,
            std::bind(&worker::on_accept, this, _1));
    }

    void
    on_accept(boost::system::error_code ec)
    {
        if( ec.failed() )
        {
            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(sect_, id(), "async_accept, error::operation_aborted");
                return;
            }

            // happens periodically, usually harmless
            LOG_DBG(sect_, id(), "async_accept ", ec.message());
            return do_accept();
        }

        // Request must be fully processed within 60 seconds.
        //request_deadline_.expires_after(
            //std::chrono::seconds(60));

        do_read();
    }

    void
    do_read()
    {
        pr_.start();

        using namespace std::placeholders;
        boost::http_io::async_read_header(
            sock_,
            pr_,
            std::bind(&worker::on_read_header,
                this, _1, _2));
    }

    void
    on_read_header(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if(ec.failed())
        {
            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(sect_, id(), "async_accept, error::operation_aborted");
                return;
            }

            // errors here are usually recoverable
            LOG_DBG(sect_, id(), "async_read_header ", ec.message());
            return do_accept();
        }

        using namespace std::placeholders;
        boost::http_io::async_read(sock_, pr_, std::bind(
            &worker::on_read_body, this, _1, _2));
    }

    void
    on_read_body(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if( ec.failed() )
        {
            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(sect_, id(), "async_read, error::operation_aborted");
                return;
            }

            // errors here are usually recoverable
            LOG_DBG(sect_, id(), "async_read ", ec.message());
            return do_accept();
        }

        res_.clear();

        if(! srv_.is_stopping())
        {
            handle_request(
                doc_root_,
                pr_.get(),
                res_,
                sr_);
        }
        else
        {
            service_unavailable(pr_.get(), res_, sr_);
        }

        using namespace std::placeholders;
        boost::http_io::async_write(sock_, sr_, std::bind(
            &worker::on_write, this, _1, _2));
    }

    void
    on_write(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if( ec.failed() )
        {
            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(sect_, id(), "async_write, error::operation_aborted");
                return;
            }

            // errors here are usually recoverable
            LOG_DBG(sect_, id(), "async_write ", ec.message());
            return do_accept();
        }

        if(res_.keep_alive())
            return do_read();

        do_accept();
    }
};

} // http_io
} // boost

#endif
