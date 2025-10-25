//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_EXAMPLE_SERVER_HANDLER_HPP
#define BOOST_BEAST2_EXAMPLE_SERVER_HANDLER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/http_handler.hpp>

namespace boost {
namespace beast2 {

struct https_redirect_responder
{
    system::error_code operator()(Request&, Response&) const;
};

} // beast2
} // boost

#endif
