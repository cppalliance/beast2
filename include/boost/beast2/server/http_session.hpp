//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_HTTP_SESSION_HPP
#define BOOST_BEAST2_SERVER_HTTP_SESSION_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/application.hpp>
#include <boost/beast2/log_service.hpp>
#include <boost/beast2/format.hpp>
#include <boost/beast2/read.hpp>
#include <boost/beast2/write.hpp>
#include <boost/beast2/server/any_lambda.hpp>
#include <boost/beast2/server/basic_router.hpp>
#include <boost/beast2/server/route_handler_asio.hpp>
#include <boost/beast2/server/router_asio.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/url/parse.hpp>
#include <boost/asio/prepend.hpp>

namespace boost {
namespace beast2 {

//------------------------------------------------
/*

*/
/** Mixin for delivering responses to HTTP requests
*/
template<class AsyncStream>
class http_session
    : private detacher::owner
{
public:
    http_session(
        application& app,
        AsyncStream& stream,
        router_asio<AsyncStream&> rr,
        any_lambda<void(system::error_code)> close_fn);

    /** Called to start a new HTTP session

        The stream must be in a connected,
        correct state for a new session.
    */
    void do_session(acceptor_config const& config);

private:
    void do_read();

    void on_read(
        system::error_code ec,
        std::size_t bytes_transferred);

    void on_write(
        system::error_code const& ec,
        std::size_t bytes_transferred);

    void do_fail(core::string_view s,
        system::error_code const& ec);

    resumer do_detach() override;

    void do_resume(system::error_code const& ec) override;

    void do_resume2(system::error_code ec);

protected:
    std::string id() const
    {
        return std::string("[") + std::to_string(id_) + "] ";
    }

protected:
    section sect_;
    std::size_t id_ = 0;
    AsyncStream& stream_;
    router_asio<AsyncStream&> rr_;
    any_lambda<void(system::error_code)> close_;
    acceptor_config const* pconfig_ = nullptr;

    using work_guard = asio::executor_work_guard<decltype(
        std::declval<AsyncStream&>().get_executor())>;
    std::unique_ptr<work_guard> pwg_;
    Request req_;
    ResponseAsio<AsyncStream&> res_;
};

//------------------------------------------------

template<class AsyncStream>
http_session<AsyncStream>::
http_session(
    application& app,
    AsyncStream& stream,
    router_asio<AsyncStream&> rr,
    any_lambda<void(system::error_code)> close)
    : sect_(use_log_service(app).get_section("http_session"))
    , id_(
        []() noexcept
        {
            static std::size_t n = 0;
            return ++n;
        }())
    , stream_(stream)
    , rr_(std::move(rr))
    , close_(close)
    , res_(stream_)
{
    req_.parser = http_proto::request_parser(app.services());

    res_.serializer = http_proto::serializer(app.services());
    res_.detach = detacher(*this);
}

/** Called to start a new HTTP session

    The stream must be in a connected,
    correct state for a new session.
*/
template<class AsyncStream>
void
http_session<AsyncStream>::
do_session(
    acceptor_config const& config)
{
    pconfig_ = &config;
    req_.parser.reset();
    do_read();
}

template<class AsyncStream>
void
http_session<AsyncStream>::
do_read()
{
    req_.parser.start();
    res_.serializer.reset();
    beast2::async_read(stream_, req_.parser,
        call_mf(&http_session::on_read, this));
}

template<class AsyncStream>
void 
http_session<AsyncStream>::
on_read(
    system::error_code ec,
    std::size_t bytes_transferred)
{
    (void)bytes_transferred;

    if(ec.failed())
        return do_fail("http_session::on_read", ec);

    LOG_TRC(this->sect_)(
        "{} http_session::on_read bytes={}",
        this->id(), bytes_transferred);

    BOOST_ASSERT(req_.parser.is_complete());

    //----------------------------------------
    //
    // set up Request and Response objects
    //

    // VFALCO HACK for now we make a copy of the message
    req_.message = req_.parser.get();

    // copy version
    res_.message.set_version(req_.message.version());

    // copy keep-alive setting
    res_.message.set_start_line(
        http_proto::status::ok, req_.parser.get().version());
    res_.message.set_keep_alive(req_.parser.get().keep_alive());

    // parse the URL
    {
        auto rv = urls::parse_uri_reference(req_.parser.get().target());
        if(rv.has_value())
        {
            req_.url = rv.value();
            req_.base_path = "";
            req_.path = std::string(rv->encoded_path());
        }
        else
        {
            // error parsing URL
            res_.status(
                http_proto::status::bad_request);
            res_.set_body(
                "Bad Request: " + rv.error().message());
            goto do_write;
        }
    }

    // invoke handlers for the route
    BOOST_ASSERT(! pwg_);
    ec = rr_.dispatch(req_.message.method(), req_.url, req_, res_);
    if(ec == route::send)
        goto do_write;

    if(ec == route::next)
    {
        // unhandled
        res_.status(http_proto::status::not_found);
        std::string s;
        format_to(s, "The requested URL {} was not found on this server.", req_.url);
        //res_.message.set_keep_alive(false); // VFALCO?
        res_.set_body(s);
        goto do_write;
    }

    if(ec == route::detach)
    {
        // make sure they called detach()
        BOOST_ASSERT(pwg_);
        return;
    }

    // error message of last resort
    {
        BOOST_ASSERT(ec.failed());
        res_.status(http_proto::status::internal_server_error);
        std::string s;
        format_to(s, "An internal server error occurred: {}", ec.message());
        //res_.message.set_keep_alive(false); // VFALCO?
        res_.set_body(s);
    }

do_write:
    if(res_.serializer.is_done())
    {
        // happens when the handler sends the response
        return on_write(system::error_code(), 0);
    }

    beast2::async_write(stream_, res_.serializer,
        call_mf(&http_session::on_write, this));
}

template<class AsyncStream>
void 
http_session<AsyncStream>::
on_write(
    system::error_code const& ec,
    std::size_t bytes_transferred)
{
    (void)bytes_transferred;

    if(ec.failed())
        return do_fail("http_session::on_write", ec);

    BOOST_ASSERT(res_.serializer.is_done());

    LOG_TRC(this->sect_)(
        "{} http_session::on_write bytes={}",
        this->id(), bytes_transferred);

    if(res_.message.keep_alive())
        return do_read();

    // tidy up lingering objects
    req_.parser.reset();
    res_.serializer.reset();
    res_.message.clear();

    close_({});
}

template<class AsyncStream>
void 
http_session<AsyncStream>::
do_fail(
    core::string_view s, system::error_code const& ec)
{
    LOG_TRC(this->sect_)("{}: {}", s, ec.message());

    // tidy up lingering objects
    req_.parser.reset();
    res_.serializer.reset();
    //res_.clear();
    //preq_.reset();

    close_(ec);
}

template<class AsyncStream>
auto
http_session<AsyncStream>::
do_detach() ->
    resumer
{
    BOOST_ASSERT(stream_.get_executor().running_in_this_thread());

    // can't call twice
    BOOST_ASSERT(! pwg_);
    pwg_.reset(new work_guard(stream_.get_executor()));

    // VFALCO cancel timer

    return resumer(*this);
}

template<class AsyncStream>
void
http_session<AsyncStream>::
do_resume(system::error_code const& ec)
{
    asio::dispatch(
        stream_.get_executor(),
        asio::prepend(call_mf(
            &http_session::do_resume2, this), ec));
}

template<class AsyncStream>
void
http_session<AsyncStream>::
do_resume2(system::error_code ec)
{
    BOOST_ASSERT(stream_.get_executor().running_in_this_thread());

    BOOST_ASSERT(pwg_.get() != nullptr);
    pwg_.reset();

    // invoke handlers for the route
    BOOST_ASSERT(! pwg_);
    ec = rr_.resume(req_, res_, ec);

    if(ec == route::detach)
    {
        // make sure they called detach()
        BOOST_ASSERT(pwg_);
        return;
    }

    if(ec.failed())
    {
        // give a default error response?
    }
    beast2::async_write(stream_, res_.serializer,
        call_mf(&http_session::on_write, this));
}

} // beast2
} // boost

#endif
