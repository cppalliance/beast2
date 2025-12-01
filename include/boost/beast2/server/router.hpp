//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTER_HPP
#define BOOST_BEAST2_SERVER_ROUTER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http_proto/server/basic_router.hpp>
#include <boost/http_proto/server/route_handler.hpp>

namespace boost {
namespace beast2 {

/** The sans-IO router type
*/
using router = http_proto::basic_router<http_proto::Request, http_proto::Response>;

} // beast2
} // boost

#endif
