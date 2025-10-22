//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_EXAMPLE_SERVER_HTTP_RESPONDER_HPP
#define BOOST_BEAST2_EXAMPLE_SERVER_HTTP_RESPONDER_HPP

#include "handler.hpp"

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/read.hpp>
#include <boost/beast2/write.hpp>
#include <boost/beast2/server/logger.hpp>
#include <boost/beast2/server/router.hpp>
#include <boost/beast2/server/server_asio.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>

namespace boost {
namespace beast2 {

//------------------------------------------------

/** Mixin for delivering responses to HTTP requests
*/
template<class Derived>
class http_responder
{
public:
    explicit
    http_responder(
        server& srv,
        router_type& rr)
        : sect_(srv.sections().get("http_responder"))
        , id_(
            []() noexcept
            {
                static std::size_t n = 0;
                return ++n;
            }())
        , rr_(rr)
        , pr_(srv.services())
        , sr_(srv.services())
    {
    }

    /** Called to start a new HTTP session

        The stream must be in a connected,
        correct state for a new session.
    */
    void do_session(
        acceptor_config const& config)
    {
        pconfig_ = &config;
        pr_.reset();
        do_read();
    }

private:
    void do_read()
    {
        pr_.start();
        beast2::async_read(self().stream(), pr_,
            call_mf(&http_responder::on_read, this));
    }

    void on_read(
        system::error_code const& ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if(ec.failed())
            return do_fail("http_responder::on_read", ec);

        LOG_TRC(this->sect_)(
            "{} http_responder::on_read bytes={}",
            this->id(), bytes_transferred);

        BOOST_ASSERT(pr_.is_complete());

        Request req{
            pr_.get().method(),
            urls::segments_encoded_view(
                pr_.get().target()),
            *this->pconfig_,
            pr_.get(),
            pr_,
            self().server().is_stopping()
        };
        Response res{
            res_,
            sr_
        };

        // invoke handlers for the route
        auto cont = rr_(req, res);
        BOOST_ASSERT(! cont);

        beast2::async_write(self().stream(), sr_,
            call_mf(&http_responder::on_write, this));
    }

    void on_write(
        system::error_code const& ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if(ec.failed())
            return do_fail("http_responder::on_write", ec);

        BOOST_ASSERT(sr_.is_done());

        LOG_TRC(this->sect_)(
            "{} http_responder::on_write bytes={}",
            this->id(), bytes_transferred);

        if(res_.keep_alive())
            return do_read();

        // tidy up lingering objects
        pr_.reset();
        sr_.reset();
        res_.clear();

        return self().do_close();
    }

    void do_fail(
        core::string_view s, system::error_code const& ec)
    {
        // tidy up lingering objects
        pr_.reset();
        sr_.reset();
        res_.clear();

        self().do_fail(s, ec);
    }

    Derived&
    self() noexcept
    {
        return *static_cast<Derived*>(this);
    }

    Derived const&
    self() const noexcept
    {
        return *static_cast<Derived const*>(this);
    }

protected:
    std::string id() const
    {
        return std::string("[") + std::to_string(id_) + "] ";
    }

protected:
    section sect_;
    std::size_t id_ = 0;
    router_type& rr_;
    http_proto::request_parser pr_;
    http_proto::serializer sr_;
    http_proto::response res_;
    acceptor_config const* pconfig_ = nullptr;
};

} // beast2
} // boost

#endif
