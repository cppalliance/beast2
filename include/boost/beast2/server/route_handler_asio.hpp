//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTE_HANDLER_ASIO_HPP
#define BOOST_BEAST2_SERVER_ROUTE_HANDLER_ASIO_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/route_handler.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

/** Response object for Asio HTTP route handlers
*/
template<class AsyncStream>
struct ResponseAsio : Response
{
    using stream_type = typename std::decay<AsyncStream>::type;

    AsyncStream stream;

    template<class... Args>
    explicit
    ResponseAsio(
        Args&&... args)
        : stream(std::forward<Args>(args)...)
    {
    }
};

} // beast2
} // boost

#endif
