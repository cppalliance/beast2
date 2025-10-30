//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_HTTP_REDIRECT_RESPONDER_HPP
#define BOOST_BEAST2_SERVER_HTTP_REDIRECT_RESPONDER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/route_handler.hpp>

namespace boost {
namespace beast2 {

struct https_redirect_responder
{
    BOOST_BEAST2_DECL
    system::error_code
    operator()(Request&, Response&) const;
};

} // beast2
} // boost

#endif
