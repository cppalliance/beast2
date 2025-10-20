//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_ROUTE_PARAMS_HPP
#define BOOST_HTTP_IO_SERVER_ROUTE_PARAMS_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/url/segments_encoded_view.hpp>

namespace boost {
namespace http_io {

struct acceptor_config
{
    bool is_ssl;
    bool is_admin;
};

/** Request object for HTTP route handlers
*/
struct Request
{
    http_proto::method method;
    urls::segments_encoded_view path;

    acceptor_config port;
    http_proto::request_base const& req;
    http_proto::request_parser& pr;
    bool is_shutting_down;
};

/** Response object for HTTP route handlers
*/
struct Response
{
    http_proto::response& res;
    http_proto::serializer& sr;

    /*
    bool send(core::string_view);
    bool error(system::error_code);
    bool status(http_proto::status);
    */
};

struct AsioResponse : Response
{
    template<class... Args>
    AsioResponse(Args&&... args)
        : Response(std::forward<Args>(args)...)
    {
    }
};

} // http_io
} // boost

#endif
