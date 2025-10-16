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
#include <boost/http_io/server/any_lambda.hpp>
#include <boost/http_io/server/call_mf.hpp>
#include <boost/http_io/server/ports.hpp>
#include <boost/http_io/server/router.hpp>
#include <boost/http_io/read.hpp>
#include <boost/http_io/ssl_stream.hpp>
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
    class Protocol = asio::ip::tcp
>
class worker_ssl : public http_responder<
    worker_ssl<Executor, Protocol>>
{
public:
    using executor_type = Executor;

    using protocol_type = Protocol;

    using socket_type =
        asio::basic_stream_socket<Protocol, Executor>;
    
    using stream_type = ssl_stream<socket_type>;

    template<
        class Executor0,
        class = typename std::enable_if<std::is_constructible<Executor, Executor0>::value>::type
    >
    worker_ssl(
        std::size_t index,
        any_lambda<void(std::size_t)> do_idle,
        asio_server& srv,
        Executor0 const& ex,
        asio::ssl::context& ssl_ctx,
        router_type& rr)
        : http_responder<worker_ssl>(srv, rr)
        , index_(index)
        , do_idle_(std::move(do_idle))
        , ssl_ctx_(ssl_ctx)
        , stream_(ex, ssl_ctx)
    {
    }

    typename ssl_stream<socket_type>::ssl_stream_type&
    stream() noexcept
    {
        return stream_.stream();
    }

    /** Cancel all outstanding I/O
    */
    void cancel()
    {
        system::error_code ec;
        stream_.next_layer().cancel(ec);
    }

    template<class Executor1>
    void do_accept(
        asio::basic_socket_acceptor<Protocol, Executor1>& acc,
        any_lambda<void()> do_accepted)
    {
        do_accepted_ = do_accepted;

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

        acc.async_accept(stream_.next_layer(), ep_,
            call_mf(&worker_ssl::on_accept, this));
    }

    /** Called when the connected session is ended
    */
    void do_close()
    {
        stream_.stream().async_shutdown(call_mf(
            &worker_ssl::on_shutdown, this));
    }

    void do_failed(
        core::string_view s, system::error_code const& ec)
    {
        if(ec == asio::error::operation_aborted)
        {
            LOG_TRC(this->sect_, this->id(), " ", s, ": ", ec.message());
            // this means the worker was stopped, don't submit new work
            return;
        }

        LOG_DBG(this->sect_, this->id(), " ", s, ": ", ec.message());
        return do_idle();
    }

private:
    void on_accept(system::error_code const& ec)
    {
        do_accepted_();

        if(ec.failed())
            return do_failed("worker_ssl::on_accept", ec);

        // Request must be fully processed within 60 seconds.
        //request_deadline_.expires_after(
            //std::chrono::seconds(60));

        using namespace std::placeholders;
        stream_.set_ssl(true);
        stream_.stream().async_handshake(
            asio::ssl::stream_base::server,
            std::bind(&worker_ssl::on_handshake, this, _1));
    }

    void on_handshake(
        system::error_code const& ec)
    {
        if(ec.failed())
            return do_failed("worker_ssl::on_handshake", ec);

        // Request must be fully processed within 60 seconds.
        //request_deadline_.expires_after(
            //std::chrono::seconds(60));
        this->do_start();
    }

    void on_shutdown(system::error_code ec)
    {
        if(ec.failed())
            return do_failed("worker_ssl::on_shutdown", ec);

        stream_.next_layer().shutdown(asio::socket_base::shutdown_both, ec);
        // error ignored

        return do_idle();
    }

    void do_idle()
    {
        do_idle_(index_);
    }

private:
    // order of destruction matters here
    std::size_t index_;
    any_lambda<void(std::size_t)> do_idle_;
    any_lambda<void()> do_accepted_;
    asio::ssl::context& ssl_ctx_;
    stream_type stream_;
    typename Protocol::endpoint ep_;
};

} // http_io
} // boost

#endif
