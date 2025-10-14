//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_HTTP_RESPONDER_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_HTTP_RESPONDER_HPP

#include "asio_server.hpp"
#include "handler.hpp"
#include "logger.hpp"

#include <boost/http_io/detail/config.hpp>
#include <boost/http_io/read.hpp>
#include <boost/http_io/write.hpp>
#include <boost/http_io/server/router.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <functional> // for _1

namespace boost {

namespace asio {
namespace ip {
class tcp; // forward declaration
} // ip
} // asio

namespace http_io {

//------------------------------------------------

/** Mixin for delivering responses to HTTP requests
*/
template<class Derived>
class http_responder
{
public:
    http_responder(
        asio_server& srv,
        router_type& rr)
        : id_(srv.make_unique_id())
        , srv_(srv)
        , rr_(rr)
        , pr_(srv_.services())
        , sr_(srv_.services())
    {
        pr_.reset();
    }

    void
    do_read_request()
    {
        pr_.start();

        using namespace std::placeholders;
        http_io::async_read_header(
            self().stream(),
            pr_,
            std::bind(&http_responder::on_read_header,
                this, _1, _2));
    }

private:
    void
    on_read_header(
        system::error_code const& ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if(ec.failed())
        {
            reset();

            if( ec == asio::error::operation_aborted )
            {
                LOG_TRC(sect_, id(), "async_accept, error::operation_aborted");
                // worker is canceled, exit the I/O loop
                return;
            }

            // errors here are usually recoverable
            LOG_DBG(sect_, id(), "async_read_header ", ec.message());
            return self().do_accept();
        }

        using namespace std::placeholders;
        http_io::async_read(self().stream(), pr_, std::bind(
            &http_responder::on_read_body, this, _1, _2));
    }

    void
    on_read_body(
        system::error_code const& ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if( ec.failed() )
        {
            reset();

            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(sect_, id(), "async_read, error::operation_aborted");
                return;
            }

            // errors here are usually recoverable
            LOG_DBG(sect_, id(), "async_read ", ec.message());
            return self().do_accept();
        }

        // find the route
        auto found = rr_.invoke(
            pr_.get().method(),
            pr_.get().target(),
            http_params{
                pr_.get(),
                res_,
                sr_,
                srv_.is_stopping()});

        if(! found)
        {
            // errors here are usually recoverable
            LOG_DBG(sect_, id(), "no route");
            reset();
            return self().do_accept();
        }

        using namespace std::placeholders;
        http_io::async_write(self().stream(), sr_, std::bind(
            &http_responder::on_write, this, _1, _2));
    }

    void
    on_write(
        system::error_code const& ec,
        std::size_t bytes_transferred)
    {
        (void)bytes_transferred;

        if( ec.failed() )
        {
            reset();

            if( ec == asio::error::operation_aborted )
            {
                // worker is canceled, exit the I/O loop
                LOG_TRC(sect_, id(), "async_write, error::operation_aborted");
                return;
            }

            // errors here are usually recoverable
            LOG_DBG(sect_, id(), "async_write ", ec.message());
            return self().do_accept();
        }

        if(res_.keep_alive())
            return do_read_request();

        reset();
        return self().do_accept();
    }

    void
    reset()
    {
        pr_.reset();
        sr_.reset();
        res_.clear();
    }

private:
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
    std::size_t id_ = 0;
    section sect_;
    asio_server& srv_;

private:
    router_type& rr_;
    http_proto::request_parser pr_;
    http_proto::serializer sr_;
    http_proto::response res_;
};

} // http_io
} // boost

#endif
