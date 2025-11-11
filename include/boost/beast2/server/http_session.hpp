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
template<class Stream>
class http_session
    : private detacher::owner
{
public:
    http_session(
        application& app,
        Stream& stream,
        router_asio<Stream> rr,
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
    Stream& stream_;
    router_asio<Stream> rr_;
    any_lambda<void(system::error_code)> close_;
    http_proto::request_parser pr_;
    http_proto::serializer sr_;
    http_proto::response res_;
    acceptor_config const* pconfig_ = nullptr;

    using work_guard = asio::executor_work_guard<decltype(
        std::declval<Stream&>().get_executor())>;
    std::unique_ptr<work_guard> pwg_;
    std::unique_ptr<Request> preq_;
    std::unique_ptr<ResponseAsio<Stream>> pres_;
};

//------------------------------------------------

template<class Stream>
http_session<Stream>::
http_session(
    application& app,
    Stream& stream,
    router_asio<Stream> rr,
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
    , pr_(app.services())
    , sr_(app.services())
{
}

/** Called to start a new HTTP session

    The stream must be in a connected,
    correct state for a new session.
*/
template<class Stream>
void
http_session<Stream>::
do_session(
    acceptor_config const& config)
{
    pconfig_ = &config;
    pr_.reset();
    do_read();
}

template<class Stream>
void
http_session<Stream>::
do_read()
{
    pr_.start();
    sr_.reset();
    beast2::async_read(stream_, pr_,
        call_mf(&http_session::on_read, this));
}

template<class Stream>
void 
http_session<Stream>::
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

    BOOST_ASSERT(pr_.is_complete());

    //----------------------------------------
    //
    // set up Request and Response objects
    //

    preq_.reset(new Request(
        *this->pconfig_,
        pr_.get(),
        pr_));

    pres_.reset(new ResponseAsio<Stream>(
        stream_,
        res_,
        sr_));
    pres_->detach = detacher(*this);

    // copy version
    pres_->m.set_version(preq_->m.version());

    // copy keep-alive setting
    pres_->m.set_start_line(
        http_proto::status::ok, pr_.get().version());
    pres_->m.set_keep_alive(pr_.get().keep_alive());

    // parse the URL
    {
        auto rv = urls::parse_uri_reference(pr_.get().target());
        if(rv.has_value())
        {
            preq_->url = rv.value();
            preq_->base_path = "";
            preq_->path = std::string(rv->encoded_path());
        }
        else
        {
            // error parsing URL
            pres_->status(
                http_proto::status::bad_request);
            pres_->set_body(
                "Bad Request: " + rv.error().message());
            goto do_write;
        }
    }

    // invoke handlers for the route
    BOOST_ASSERT(! pwg_);
    ec = rr_.dispatch(
        preq_->m.method(),
        preq_->url,
        *preq_, *pres_);
    if(ec == route::send)
        goto do_write;

    if(ec == route::next)
    {
        // unhandled
        pres_->status(http_proto::status::not_found);
        std::string s;
        format_to(s, "The requested URL {} was not found on this server.", preq_->url);
        //pres_->m.set_keep_alive(false); // VFALCO?
        pres_->set_body(s);
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
        pres_->status(http_proto::status::internal_server_error);
        std::string s;
        format_to(s, "An internal server error occurred: {}", ec.message());
        //pres_->m.set_keep_alive(false); // VFALCO?
        pres_->set_body(s);
    }

do_write:
    if(sr_.is_done())
    {
        // happens when the handler sends the response
        return on_write(system::error_code(), 0);
    }

    beast2::async_write(stream_, sr_,
        call_mf(&http_session::on_write, this));
}

template<class Stream>
void 
http_session<Stream>::
on_write(
    system::error_code const& ec,
    std::size_t bytes_transferred)
{
    (void)bytes_transferred;

    if(ec.failed())
        return do_fail("http_session::on_write", ec);

    BOOST_ASSERT(sr_.is_done());

    LOG_TRC(this->sect_)(
        "{} http_session::on_write bytes={}",
        this->id(), bytes_transferred);

    if(res_.keep_alive())
        return do_read();

    // tidy up lingering objects
    pr_.reset();
    sr_.reset();
    res_.clear();
    preq_.reset();
    pres_.reset();

    close_({});
}

template<class Stream>
void 
http_session<Stream>::
do_fail(
    core::string_view s, system::error_code const& ec)
{
    LOG_TRC(this->sect_)("{}: {}", s, ec.message());

    // tidy up lingering objects
    pr_.reset();
    sr_.reset();
    res_.clear();
    preq_.reset();
    pres_.reset();

    close_(ec);
}

template<class Stream>
auto
http_session<Stream>::
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

template<class Stream>
void
http_session<Stream>::
do_resume(system::error_code const& ec)
{
    asio::dispatch(
        stream_.get_executor(),
        asio::prepend(call_mf(
            &http_session::do_resume2, this), ec));
}

template<class Stream>
void
http_session<Stream>::
do_resume2(system::error_code ec)
{
    BOOST_ASSERT(stream_.get_executor().running_in_this_thread());

    BOOST_ASSERT(pwg_.get() != nullptr);
    pwg_.reset();

    // invoke handlers for the route
    BOOST_ASSERT(! pwg_);
    ec = rr_.resume(*preq_, *pres_, ec);

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
    beast2::async_write(stream_, sr_,
        call_mf(&http_session::on_write, this));
}

} // beast2
} // boost

#endif
