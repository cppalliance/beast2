//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_COROSIO_ROUTER_HPP
#define BOOST_BEAST2_SERVER_COROSIO_ROUTER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http/server/router.hpp>
#include <boost/http/field.hpp>
#include <boost/http/string_body.hpp>
#include <boost/corosio/socket.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/buffers/buffer_copy.hpp>
#include <boost/capy/write.hpp>
#include <span>

namespace boost {
namespace beast2 {

/** Route parameters object for Corosio HTTP route handlers
*/
class corosio_route_params
    : public http::route_params
{
public:
    using stream_type = corosio::socket;

    corosio::io_stream& stream;

    explicit
    corosio_route_params(
        corosio::io_stream& s)
        : stream(s)
    {}
};

/** The Corosio-aware router type
*/
using corosio_router = http::basic_router<corosio_route_params>;

} // beast2
} // boost

#endif
