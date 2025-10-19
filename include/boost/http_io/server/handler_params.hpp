//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_HANDLER_PARAMS_HPP
#define BOOST_HTTP_IO_SERVER_HANDLER_PARAMS_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>

namespace boost {
namespace http_io {

struct acceptor_config
{
    bool is_ssl;
    bool is_admin;
};

/** Parameters passed to a request handler

    Objects of this type are passed to handlers which respond to HTTP requests.
*/
struct handler_params
{
    acceptor_config const& port;

    http_proto::request_base const& req;
    http_proto::response& res;

    http_proto::request_parser& parser;
    http_proto::serializer& serializer;

    bool is_shutting_down;
};

#if 0
struct resumer
{
    struct impl
    {
        ~impl() = default;
        virtual void resume() = 0;
        virtual void close() = 0;
        virtual void fail() = 0;
    };

    /** Destructor

        If no other members have been invoked, destruction
        of the resumer object is equivalent to calling close().
    */
    ~resumer();

    resumer(std::shared_ptr<impl> sp)
        : sp_(std::move(sp))
    {
    }

    void resume()
    {
        sp_->resume();
    }

    void close()
    {
        sp_->close();
    }

    void fail()
    {
        sp_->fail();
    }

private:
    std::shared_ptr<impl> sp_;
};

struct actions
{
    ~actions() = default;
    virtual void detach() = 0;
    virtual void close() = 0;
    virtual void fail() = 0;
};
#endif

} // http_io
} // boost

#endif
