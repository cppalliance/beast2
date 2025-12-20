//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_SERVE_REDIRECT_HPP
#define BOOST_BEAST2_SERVER_SERVE_REDIRECT_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http_proto/server/route_handler.hpp>

namespace boost {
namespace beast2 {

struct serve_redirect
{
    BOOST_BEAST2_DECL
    http::route_result
    operator()(
        http::route_params&) const;
};

} // beast2
} // boost

#endif
