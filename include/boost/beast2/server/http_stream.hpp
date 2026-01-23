//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_HTTP_STREAM_HPP
#define BOOST_BEAST2_SERVER_HTTP_STREAM_HPP

#if 0

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/log_service.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/format.hpp>
#include <boost/beast2/server/route_handler_corosio.hpp>
#include <boost/beast2/server/router_corosio.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/http/application.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/corosio/socket.hpp>
#include <boost/corosio/io_result.hpp>
#include <boost/http/request_parser.hpp>
#include <boost/http/response.hpp>
#include <boost/http/serializer.hpp>
#include <boost/http/string_body.hpp>
#include <boost/http/server/flat_router.hpp>
#include <boost/http/server/router.hpp>
#include <boost/http/error.hpp>
#include <boost/url/parse.hpp>

namespace boost {
namespace beast2 {

//------------------------------------------------

/** An HTTP server stream which routes requests to handlers and sends responses.

    This class provides a coroutine-based HTTP server session that reads
    HTTP requests, routes them to handlers installed in a router, and
    sends the HTTP response.

    The session runs as a coroutine task and uses Corosio for async I/O.
*/
class http_stream
{
public:
    /** Constructor.
        
        Initializes a new HTTP connection object that operates on
        the given socket and uses the specified router to dispatch
        incoming requests.

        @param app The owning application, used to access shared services
            such as logging and protocol objects.
        @param sock The socket to read from and write to.
        @param routes The router used to dispatch incoming HTTP requests.
    */
    http_stream(
        http::application& app,
        corosio::socket& sock,
        http::flat_router& routes);

    /** Run the HTTP session as a coroutine.

        Reads HTTP requests, dispatches them through the router,
        and writes responses until the connection closes or an
        error occurs.

        @param config The acceptor configuration for this connection.

        @return A task that completes when the session ends.
    */
    capy::task<void>
    run(http::acceptor_config const& config);

private:
    capy::task<corosio::io_result<std::size_t>>
    read_header();

    capy::task<corosio::io_result<std::size_t>>
    read_body();

    capy::task<corosio::io_result<std::size_t>>
    write_response();

    void on_headers();
    http::route_result do_dispatch();
    void do_respond(http::route_result rv);

    std::string id() const
    {
        return std::string("[") + std::to_string(id_) + "] ";
    }

    void clear() noexcept;

    section sect_;
    std::size_t id_ = 0;
    corosio::socket& sock_;
    //http::flat_router& routes_;
    http::acceptor_config const* pconfig_ = nullptr;
    corosio_route_params rp_;
};

//------------------------------------------------

inline
http_stream::
http_stream(
    http::application& app,
    corosio::socket& sock,
    http::flat_router& /*routes*/)
    : sect_(use_log_service(app).get_section("http_stream"))
    , id_(
        []() noexcept
        {
            static std::size_t n = 0;
            return ++n;
        }())
    , sock_(sock)
    //, routes_(routes)
    , rp_(sock_)
{
    rp_.parser = http::request_parser(app);
    rp_.serializer = http::serializer(app);
    // Note: suspend mechanism removed - handlers must complete synchronously
}

inline
capy::task<void>
http_stream::
run(http::acceptor_config const& config)
{
    pconfig_ = &config;

    for (;;)
    {
        // Reset parser for new request
        rp_.parser.reset();
        rp_.session_data.clear();
        rp_.parser.start();

        // Read HTTP request header
        auto [ec, n] = co_await read_header();
        if (ec)
        {
            LOG_TRC(sect_)("{} read_header: {}", id(), ec.message());
            break;
        }

        LOG_TRC(sect_)("{} read_header bytes={}", id(), n);

        // Process headers and dispatch
        on_headers();

        auto rv = do_dispatch();
        do_respond(rv);

        // Write response
        if (!rp_.serializer.is_done())
        {
            auto [wec, wn] = co_await write_response();
            if (wec)
            {
                LOG_TRC(sect_)("{} write_response: {}", id(), wec.message());
                break;
            }
            LOG_TRC(sect_)("{} write_response bytes={}", id(), wn);
        }

        // Check keep-alive
        if (!rp_.res.keep_alive())
            break;
    }

    clear();
}

inline
capy::task<corosio::io_result<std::size_t>>
http_stream::
read_header()
{
    std::size_t total_bytes = 0;
    system::error_code ec;

    for (;;)
    {
        // Try to parse what we have
        rp_.parser.parse(ec);
        
        if (ec == http::condition::need_more_input)
        {
            // Need to read more data
            auto buf = rp_.parser.prepare();
            auto [read_ec, n] = co_await sock_.read_some(buf);
            
            if (read_ec)
            {
                co_return {read_ec, total_bytes};
            }
            
            if (n == 0)
            {
                // EOF
                rp_.parser.commit_eof();
                ec = {};
            }
            else
            {
                rp_.parser.commit(n);
                total_bytes += n;
            }
            continue;
        }

        if (ec.failed())
        {
            co_return {ec, total_bytes};
        }

        // Header complete
        if (rp_.parser.got_header())
        {
            co_return {{}, total_bytes};
        }
    }
}

inline
capy::task<corosio::io_result<std::size_t>>
http_stream::
read_body()
{
    std::size_t total_bytes = 0;
    system::error_code ec;

    while (!rp_.parser.is_complete())
    {
        rp_.parser.parse(ec);
        
        if (ec == http::condition::need_more_input)
        {
            auto buf = rp_.parser.prepare();
            auto [read_ec, n] = co_await sock_.read_some(buf);
            
            if (read_ec)
            {
                co_return {read_ec, total_bytes};
            }
            
            if (n == 0)
            {
                rp_.parser.commit_eof();
            }
            else
            {
                rp_.parser.commit(n);
                total_bytes += n;
            }
            continue;
        }

        if (ec.failed())
        {
            co_return {ec, total_bytes};
        }
    }

    co_return {{}, total_bytes};
}

inline
capy::task<corosio::io_result<std::size_t>>
http_stream::
write_response()
{
    std::size_t total_bytes = 0;

    while (!rp_.serializer.is_done())
    {
        auto rv = rp_.serializer.prepare();
        if (!rv)
        {
            co_return {rv.error(), total_bytes};
        }

        auto bufs = *rv;
        std::size_t buf_size = 0;
        for (auto const& buf : bufs)
            buf_size += buf.size();

        if (buf_size == 0)
        {
            // Serializer done
            break;
        }

        // Write buffers - we need to write them all
        std::size_t written = 0;
        for (auto const& buf : bufs)
        {
            auto [ec, n] = co_await sock_.write_some(
                capy::const_buffer(buf.data(), buf.size()));
            if (ec)
            {
                co_return {ec, total_bytes + written};
            }
            written += n;
        }

        rp_.serializer.consume(written);
        total_bytes += written;
    }

    co_return {{}, total_bytes};
}

inline
void
http_stream::
on_headers()
{
    // Set up Request and Response objects
    rp_.req = rp_.parser.get();
    rp_.route_data.clear();
    rp_.res.set_start_line(
        http::status::ok, rp_.req.version());
    rp_.res.set_keep_alive(rp_.req.keep_alive());
    rp_.serializer.reset();

    // Parse the URL
    auto rv = urls::parse_uri_reference(rp_.req.target());
    if (rv.has_error())
    {
        rp_.status(http::status::bad_request);
        rp_.set_body("Bad Request: " + rv.error().message());
        return;
    }

    rp_.url = rv.value();
}

inline
http::route_result
http_stream::
do_dispatch()
{
    return {};
    //return routes_.dispatch(
        //rp_.req.method(), rp_.url, rp_);
}

inline
void
http_stream::
do_respond(http::route_result rv)
{
    if (rv == http::route::close)
    {
        rp_.res.set_keep_alive(false);
        return;
    }

    if (rv == http::route::complete)
    {
        // Handler sent the response directly
        return;
    }

    if (rv == http::route::suspend)
    {
        // Suspend not supported - treat as internal error
        rp_.status(http::status::internal_server_error);
        rp_.set_body("Handler suspend not supported");
        rp_.res.set_keep_alive(false);
        return;
    }

    if (rv == http::route::next)
    {
        // Unhandled request
        auto const status = http::status::not_found;
        rp_.status(status);
        rp_.set_body(http::to_string(status));
        return;
    }

    if (rv != http::route::send)
    {
        // Error message of last resort
        BOOST_ASSERT(rv.failed());
        BOOST_ASSERT(!http::is_route_result(rv));
        rp_.status(http::status::internal_server_error);
        std::string s;
        format_to(s, "An internal server error occurred: {}", rv.message());
        rp_.res.set_keep_alive(false);
        rp_.set_body(s);
    }
}

inline
void
http_stream::
clear() noexcept
{
    rp_.parser.reset();
    rp_.serializer.reset();
    rp_.res.clear();
}

} // beast2
} // boost

#endif

#endif
