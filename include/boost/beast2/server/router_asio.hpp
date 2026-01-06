//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTER_ASIO_HPP
#define BOOST_BEAST2_SERVER_ROUTER_ASIO_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http/server/basic_router.hpp>
#include <boost/beast2/server/route_handler_asio.hpp>

namespace boost {
namespace beast2 {

/** The Asio-aware router type
*/
template<class Stream>
using router_asio = http::basic_router<
    asio_route_params<Stream>>;

} // beast2
} // boost

#endif
