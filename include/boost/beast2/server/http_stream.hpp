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

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/log_service.hpp>
#include <boost/beast2/format.hpp>
#include <boost/beast2/read.hpp>
#include <boost/beast2/write.hpp>
#include <boost/beast2/server/any_lambda.hpp>
#include <boost/beast2/server/route_handler_asio.hpp>
#include <boost/beast2/server/router_asio.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/capy/application.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/http_proto/server/basic_router.hpp>
#include <boost/url/parse.hpp>
#include <boost/asio/prepend.hpp>

namespace boost {
namespace beast2 {

//------------------------------------------------

/** An HTTP server stream which routes requests to handlers and sends responses.

    An object of this type wraps an asynchronous Boost.ASIO stream and implements
    a high level server connection which reads HTTP requests, routes them to
    handlers installed in a router, and sends the HTTP response.

    @par Requires
    `AsyncStream` must satisfy <em>AsyncReadStream</em> and <em>AsyncWriteStream</em>

    @tparam AsyncStream The type of asynchronous stream.
*/
template<class AsyncStream>
class http_stream
    : private http_proto::detacher::owner
{
public:
    /** Constructor.
        
        This initializes a new HTTP connection object that operates on
        the given stream, uses the specified router to dispatch incoming
        requests, and calls the supplied completion function when the
        connection closes or fails.

        Construction does not start any I/O; call @ref on_stream_begin when
        the stream is connected to the remote peer to begin reading
        requests and processing them.

        @param app The owning application, used to access shared services
            such as logging and protocol objects.
        @param stream The underlying asynchronous stream to read from
            and write to. The caller is responsible for maintaining its
            lifetime for the duration of the session.
        @param routes The router used to dispatch incoming HTTP requests.
        @param close_fn The function invoked when the connection is closed
            or an unrecoverable error occurs.
    */
    http_stream(
        capy::application& app,
        AsyncStream& stream,
        router_asio<AsyncStream&> routes,
        any_lambda<void(system::error_code)> close_fn);

    /** Called to start a new HTTP session

        The stream must be in a connected,
        correct state for a new session.
    */
    void on_stream_begin(http_proto::acceptor_config const& config);

private:
    void do_read();
    void on_read(
        system::error_code ec,
        std::size_t bytes_transferred);
    void on_headers();
    void do_dispatch(http_proto::route_result rv = {});
    void do_respond(http_proto::route_result rv);
    void do_write();
    void on_write(
        system::error_code const& ec,
        std::size_t bytes_transferred);
    void on_complete();
    http_proto::resumer do_detach() override;
    void do_resume(http_proto::route_result const& ec) override;
    void do_close();
    void do_fail(core::string_view s,
        system::error_code const& ec);
    void clear() noexcept;

protected:
    std::string id() const
    {
        return std::string("[") + std::to_string(id_) + "] ";
    }

protected:
    struct resetter;
    section sect_;
    std::size_t id_ = 0;
    AsyncStream& stream_;
    router_asio<AsyncStream&> routes_;
    any_lambda<void(system::error_code)> close_;
    http_proto::acceptor_config const* pconfig_ = nullptr;

    using work_guard = asio::executor_work_guard<decltype(
        std::declval<AsyncStream&>().get_executor())>;
    std::unique_ptr<work_guard> pwg_;
    asio_route_params<AsyncStream&> p_;
};

//------------------------------------------------

// for exception safety
template<class AsyncStream>
struct http_stream<AsyncStream>::
    resetter
{
    ~resetter()
    {
        if(clear_)
            owner_.clear();
    }

    explicit resetter(
        http_stream<AsyncStream>& owner) noexcept
        : owner_(owner)
    {
    }

    void accept()
    {
        clear_ = false;
    }

private:
    http_stream<AsyncStream>& owner_;
    bool clear_ = true;
};

//------------------------------------------------

template<class AsyncStream>
http_stream<AsyncStream>::
http_stream(
    capy::application& app,
    AsyncStream& stream,
    router_asio<AsyncStream&> routes,
    any_lambda<void(system::error_code)> close)
    : sect_(use_log_service(app).get_section("http_stream"))
    , id_(
        []() noexcept
        {
            static std::size_t n = 0;
            return ++n;
        }())
    , stream_(stream)
    , routes_(std::move(routes))
    , close_(close)
    , p_(stream_)
{
    p_.parser = http_proto::request_parser(app);

    p_.serializer = http_proto::serializer(app);
    p_.detach = http_proto::detacher(*this);
}

// called to start a new HTTP session.
// the connection must be in the correct state already.
template<class AsyncStream>
void
http_stream<AsyncStream>::
on_stream_begin(
    http_proto::acceptor_config const& config)
{
    pconfig_ = &config;

    p_.parser.reset();
    p_.session_data.clear();
    do_read();
}

// begin reading the request
template<class AsyncStream>
void
http_stream<AsyncStream>::
do_read()
{
    p_.parser.start();

    beast2::async_read(stream_, p_.parser,
        call_mf(&http_stream::on_read, this));
}

// called when the read operation completes
template<class AsyncStream>
void 
http_stream<AsyncStream>::
on_read(
    system::error_code ec,
    std::size_t bytes_transferred)
{
    (void)bytes_transferred;

    if(ec.failed())
        return do_fail("http_stream::on_read", ec);

    LOG_TRC(this->sect_)(
        "{} http_stream::on_read bytes={}",
        this->id(), bytes_transferred);

    BOOST_ASSERT(p_.parser.is_complete());

    on_headers();
}

// called to set up the response after reading the request
template<class AsyncStream>
void 
http_stream<AsyncStream>::
on_headers()
{
    // set up Request and Response objects
    // VFALCO HACK for now we make a copy of the message
    p_.req = p_.parser.get();
    p_.route_data.clear();
    p_.res.set_start_line( // VFALCO WTF
        http_proto::status::ok, p_.req.version());
    p_.res.set_keep_alive(p_.req.keep_alive());
    p_.serializer.reset();

    // parse the URL
    {
        auto rv = urls::parse_uri_reference(p_.req.target());
        if(rv.has_error())
        {
            // error parsing URL
            p_.status(http_proto::status::bad_request);
            p_.set_body("Bad Request: " + rv.error().message());
            return do_respond(rv.error());
        }

        p_.url = rv.value();
    }

    // invoke handlers for the route
    do_dispatch();
}

// called to dispatch or resume the route
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_dispatch(
    http_proto::route_result rv)
{
    if(! rv.failed())
    {
        BOOST_ASSERT(! pwg_); // can't be detached
        rv = routes_.dispatch(
            p_.req.method(), p_.url, p_);
    }
    else
    {
        rv = routes_.resume(p_, rv);
    }

    do_respond(rv);
}

// called after obtaining a route result
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_respond(
    http_proto::route_result rv)
{
    BOOST_ASSERT(rv != http_proto::route::next_route);

    if(rv == http_proto::route::close)
    {
        return do_close();
    }

    if(rv == http_proto::route::complete)
    {
        // VFALCO what if the connection was closed or keep-alive=false?
        // handler sendt the response?
        BOOST_ASSERT(p_.serializer.is_done());
        return on_write(system::error_code(), 0);
    }

    if(rv == http_proto::route::detach)
    {
        // didn't call detach()?
        if(! pwg_)
            detail::throw_logic_error();
        return;
    }

    if(rv == http_proto::route::next)
    {
        // unhandled request
        auto const status = http_proto::status::not_found;
        p_.status(status);
        p_.set_body(http_proto::to_string(status));
    }
    else if(rv != http_proto::route::send)
    {
        // error message of last resort
        BOOST_ASSERT(rv.failed());
        BOOST_ASSERT(! http_proto::is_route_result(rv));
        p_.status(http_proto::status::internal_server_error);
        std::string s;
        format_to(s, "An internal server error occurred: {}", rv.message());
        p_.res.set_keep_alive(false); // VFALCO?
        p_.set_body(s);
    }

    do_write();
}

// begin writing the response
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_write()
{
    BOOST_ASSERT(! p_.serializer.is_done());
    beast2::async_write(stream_, p_.serializer,
        call_mf(&http_stream::on_write, this));
}

// called when the write operation completes
template<class AsyncStream>
void 
http_stream<AsyncStream>::
on_write(
    system::error_code const& ec,
    std::size_t bytes_transferred)
{
    (void)bytes_transferred;

    if(ec.failed())
        return do_fail("http_stream::on_write", ec);

    BOOST_ASSERT(p_.serializer.is_done());

    LOG_TRC(this->sect_)(
        "{} http_stream::on_write bytes={}",
        this->id(), bytes_transferred);

    if(p_.res.keep_alive())
        return do_read();

    do_close();
}

template<class AsyncStream>
auto
http_stream<AsyncStream>::
do_detach() ->
    http_proto::resumer
{
    BOOST_ASSERT(stream_.get_executor().running_in_this_thread());

    // can't call twice
    BOOST_ASSERT(! pwg_);
    pwg_.reset(new work_guard(stream_.get_executor()));

    // VFALCO cancel timer

    return http_proto::resumer(*this);
}

// called by resume(rv)
template<class AsyncStream>
void
http_stream<AsyncStream>::
do_resume(
    http_proto::route_result const& rv)
{
    asio::dispatch(
        stream_.get_executor(),
        [this, rv]
        {
            BOOST_ASSERT(pwg_.get() != nullptr);
            pwg_.reset();

            do_dispatch(rv);
        });
}

// called when a non-recoverable error occurs
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_fail(
    core::string_view s, system::error_code const& ec)
{
    LOG_TRC(this->sect_)("{}: {}", s, ec.message());

    // tidy up lingering objects
    p_.parser.reset();
    p_.serializer.reset();

    close_(ec);
}

// end the session
template<class AsyncStream>
void
http_stream<AsyncStream>::
do_close()
{
    clear();
    close_({});
}

// clear everything, releasing transient objects
template<class AsyncStream>
void
http_stream<AsyncStream>::
clear() noexcept
{
    p_.parser.reset();
    p_.serializer.reset();
    p_.res.clear();
}

} // beast2
} // boost

#endif
