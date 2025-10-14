//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_WORKER_SSL_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_WORKER_SSL_HPP

#include "asio_server.hpp"
#include "handler.hpp"
#include "logger.hpp"
#include <boost/http_io/server/router.hpp>
#include <boost/http_io/read.hpp>
#include <boost/http_io/write.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <stddef.h>
#include <string>

#include <functional> // for _1

namespace boost {

namespace asio {
namespace ip {
class tcp; // forward declaration
} // ip
} // asio

namespace http_io {

template<
    class Executor,
    class Executor1 = Executor,
    class Protocol = asio::ip::tcp
>
class worker_ssl : public http_responder<
    worker_ssl<Executor, Executor1, Protocol>>
{
public:
    using executor_type = Executor;

    using protocol_type = Protocol;

    using acceptor_type =
        asio::basic_socket_acceptor<Protocol, Executor1>;

    using socket_type =
        asio::basic_stream_socket<Protocol, Executor>;
    
    using stream_type = asio::ssl::stream<socket_type>;

    template<
        class Executor_
        , class = typename std::enable_if<std::is_constructible<Executor, Executor_>::value>::type
    >
    worker_ssl(
        asio_server& srv,
        acceptor_type& acc,
        Executor_ const& ex,
        asio::ssl::context& ssl_ctx,
        router_type& rr)
        : http_responder<worker_ssl>(srv, rr)
        , acc_(acc)
        , ssl_ctx_(ssl_ctx)
        , stream_(ex, ssl_ctx)
    {
    }

    stream_type&
    stream() noexcept
    {
        return stream_;
    }

    /** Run the worker I/O loop

        The worker will continue to run until stop is called
        or the worker is destroyed, whichever comes first.
    */
    void
    run()
    {
        // VFALCO Without this, run() on different workers cannot be called concurrently
        //asio::dispatch(stream_.get_executor(), std::bind(&worker_ssl::do_accept0, this));

        do_accept();
    }

    void
    stop()
    {
        system::error_code ec;
        stream_.next_layer().cancel(ec);
    }

    void
    do_accept()
    {
        // Clean up any previous connection.
        {
            system::error_code ec;
            stream_.next_layer().close(ec);

            // asio::ssl::stream has an internal state which cannot be reset.
            // In order to perform the handshake again, we destroy the old
            // object and assign a new one, in a way that preserves the
            // original socket to avoid churning file handles.
            //
            stream_ = stream_type(std::move(stream_.next_layer()), ssl_ctx_);
        }

        using namespace std::placeholders;
        acc_.async_accept( stream_.next_layer(), ep_,
            std::bind(&worker_ssl::on_accept, this, _1));
    }

private:
    void
    fail(
        std::string what,
        system::error_code ec)
    {
        if( ec == asio::error::operation_aborted )
            return;

        if( ec == asio::error::eof )
        {
            stream_.next_layer().shutdown(
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
    on_accept(system::error_code const& ec)
    {
        if( ec.failed() )
        {
            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(this->sect_, this->id(), "async_accept, error::operation_aborted");
                return;
            }

            // happens periodically, usually harmless
            LOG_DBG(this->sect_, this->id(), "async_accept ", ec.message());
            return do_accept();
        }

        // Request must be fully processed within 60 seconds.
        //request_deadline_.expires_after(
            //std::chrono::seconds(60));

        using namespace std::placeholders;
        stream_.async_handshake(
            asio::ssl::stream_base::server,
            std::bind(&worker_ssl::on_handshake, this, _1));
    }

    void
    on_handshake(
        system::error_code const& ec)
    {
        if( ec.failed() )
        {
            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(this->sect_, this->id(), "async_accept, error::operation_aborted");
                return;
            }

            // happens periodically, usually harmless
            LOG_DBG(this->sect_, this->id(), "async_accept ", ec.message());
            return do_accept();
        }

        // Request must be fully processed within 60 seconds.
        //request_deadline_.expires_after(
            //std::chrono::seconds(60));

        this->do_read_request();
    }

private:
    // order of destruction matters here
    acceptor_type& acc_;
    asio::ssl::context& ssl_ctx_;
    stream_type stream_;
    typename Protocol::endpoint ep_;
};

} // http_io
} // boost

#endif
