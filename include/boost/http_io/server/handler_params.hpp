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

/** Parameters passed to a request handler

    Objects of this type are passed to handlers which respond to HTTP requests.
*/
struct handler_params
{
    http_proto::request_base const& req;
    http_proto::response& res;

    http_proto::request_parser& parser;
    http_proto::serializer& serializer;

    bool is_shutting_down;
};

} // http_io
} // boost

#endif
