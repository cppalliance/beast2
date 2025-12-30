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
#include <boost/beast2/error.hpp>
#include <boost/beast2/format.hpp>
#include <boost/beast2/read.hpp>
#include <boost/beast2/server/any_lambda.hpp>
#include <boost/beast2/server/route_handler_asio.hpp>
#include <boost/beast2/server/router_asio.hpp>
#include <boost/beast2/wrap_executor.hpp>
#include <boost/beast2/write.hpp>
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
    : private http::suspender::owner
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
    void on_stream_begin(http::acceptor_config const& config);

private:
    void do_read();
    void on_read(
        system::error_code ec,
        std::size_t bytes_transferred);
    void on_headers();
    void do_dispatch(http::route_result rv = {});
    void do_read_body();
    void on_read_body(
        system::error_code ec,
        std::size_t bytes_transferred);
    void do_respond(http::route_result rv);
    void do_write();
    void on_write(
        system::error_code const& ec,
        std::size_t bytes_transferred);
    void on_complete();
    http::resumer do_suspend() override;
    void do_resume(http::route_result const& rv) override;
    void do_resume(std::exception_ptr ep) override;
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
    http::acceptor_config const* pconfig_ = nullptr;

    using work_guard = asio::executor_work_guard<decltype(
        std::declval<AsyncStream&>().get_executor())>;
    std::unique_ptr<work_guard> pwg_;
    asio_route_params<AsyncStream&> rp_;
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
    , rp_(stream_)
{
    rp_.parser = http::request_parser(app);

    rp_.serializer = http::serializer(app);
    rp_.suspend = http::suspender(*this);
    rp_.ex = wrap_executor(stream_.get_executor());
}

// called to start a new HTTP session.
// the connection must be in the correct state already.
template<class AsyncStream>
void
http_stream<AsyncStream>::
on_stream_begin(
    http::acceptor_config const& config)
{
    pconfig_ = &config;

    rp_.parser.reset();
    rp_.session_data.clear();
    do_read();
}

// begin reading the request
template<class AsyncStream>
void
http_stream<AsyncStream>::
do_read()
{
    rp_.parser.start();

    beast2::async_read_some(
        stream_,
        rp_.parser,
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
    rp_.req = rp_.parser.get();
    rp_.route_data.clear();
    rp_.res.set_start_line( // VFALCO WTF
        http::status::ok, rp_.req.version());
    rp_.res.set_keep_alive(rp_.req.keep_alive());
    rp_.serializer.reset();

    // parse the URL
    {
        auto rv = urls::parse_uri_reference(rp_.req.target());
        if(rv.has_error())
        {
            // error parsing URL
            rp_.status(http::status::bad_request);
            rp_.set_body("Bad Request: " + rv.error().message());
            return do_respond(rv.error());
        }

        rp_.url = rv.value();
    }

    // invoke handlers for the route
    do_dispatch();
}

// called to dispatch or resume the route
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_dispatch(
    http::route_result rv)
{
    if(! rv.failed())
    {
        BOOST_ASSERT(! pwg_); // can't be suspended
        rv = routes_.dispatch(
            rp_.req.method(), rp_.url, rp_);
    }
    else
    {
        rv = routes_.resume(rp_, rv);
    }

    do_respond(rv);
}

// finish reading the body
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_read_body()
{
    beast2::async_read(
        stream_,
        rp_.parser,
        call_mf(&http_stream::on_read_body, this));
}

// called repeatedly when reading the body
template<class AsyncStream>
void 
http_stream<AsyncStream>::
on_read_body(
    system::error_code ec,
    std::size_t bytes_transferred)
{
    if(ec.failed())
        return do_fail("http_stream::on_read_body", ec);

    LOG_TRC(this->sect_)(
        "{} http_stream::on_read_body bytes={}",
        this->id(), bytes_transferred);

    BOOST_ASSERT(rp_.parser.is_complete());

    rp_.do_finish();
}

// called after obtaining a route result
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_respond(
    http::route_result rv)
{
    BOOST_ASSERT(rv != http::route::next_route);

    if(rv == http::route::close)
    {
        return do_close();
    }

    if(rv == http::route::complete)
    {
        // VFALCO what if the connection was closed or keep-alive=false?
        // handler sent the response?
        BOOST_ASSERT(rp_.serializer.is_done());
        return on_write(system::error_code(), 0);
    }

    if(rv == http::route::suspend)
    {
        // didn't call suspend()?
        if(! pwg_)
            detail::throw_logic_error();
        if(rp_.parser.is_body_set())
            return do_read_body();
        return;
    }

    if(rv == http::route::next)
    {
        // unhandled request
        auto const status = http::status::not_found;
        rp_.status(status);
        rp_.set_body(http::to_string(status));
    }
    else if(rv != http::route::send)
    {
        // error message of last resort
        BOOST_ASSERT(rv.failed());
        BOOST_ASSERT(! http::is_route_result(rv));
        rp_.status(http::status::internal_server_error);
        std::string s;
        format_to(s, "An internal server error occurred: {}", rv.message());
        rp_.res.set_keep_alive(false); // VFALCO?
        rp_.set_body(s);
    }

    do_write();
}

// begin writing the response
template<class AsyncStream>
void 
http_stream<AsyncStream>::
do_write()
{
    BOOST_ASSERT(! rp_.serializer.is_done());
    beast2::async_write(stream_, rp_.serializer,
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

    BOOST_ASSERT(rp_.serializer.is_done());

    LOG_TRC(this->sect_)(
        "{} http_stream::on_write bytes={}",
        this->id(), bytes_transferred);

    if(rp_.res.keep_alive())
        return do_read();

    do_close();
}

template<class AsyncStream>
auto
http_stream<AsyncStream>::
do_suspend() ->
    http::resumer
{
    BOOST_ASSERT(stream_.get_executor().running_in_this_thread());

    // can't call twice
    BOOST_ASSERT(! pwg_);
    pwg_.reset(new work_guard(stream_.get_executor()));

    // VFALCO cancel timer

    return http::resumer(*this);
}

// called by resume(rv)
template<class AsyncStream>
void
http_stream<AsyncStream>::
do_resume(
    http::route_result const& rv)
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

// called by resume(ep)
template<class AsyncStream>
void
http_stream<AsyncStream>::
do_resume(
    std::exception_ptr ep)
{
    asio::dispatch(
        stream_.get_executor(),
        [this, ep]
        {
            BOOST_ASSERT(pwg_.get() != nullptr);
            pwg_.reset();

            rp_.status(http::status::internal_server_error);
            try
            {
                std::rethrow_exception(ep);
            }
            catch(std::exception const& e)
            {
                std::string s;
                format_to(s, "An internal server error occurred: {}", e.what());
                rp_.set_body(s);
            }
            catch(...)
            {
                rp_.set_body("An internal server error occurred");
            }
            rp_.res.set_keep_alive(false);
            do_write();
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
    rp_.parser.reset();
    rp_.serializer.reset();

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
    rp_.parser.reset();
    rp_.serializer.reset();
    rp_.res.clear();
}

} // beast2
} // boost

#endif
