//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTE_HANDLER_COROSIO_HPP
#define BOOST_BEAST2_SERVER_ROUTE_HANDLER_COROSIO_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http/server/route_handler.hpp>
#include <boost/corosio/socket.hpp>

namespace boost {
namespace beast2 {

/** Route parameters object for Corosio HTTP route handlers
*/
class corosio_route_params
    : public http::route_params
{
public:
    using stream_type = corosio::socket;

    corosio::socket& stream;

    explicit
    corosio_route_params(
        corosio::socket& s)
        : stream(s)
    {
    }

    void do_finish()
    {
        if(finish_)
        {
            auto f = std::move(finish_);
            finish_ = {};
            f();
        }
    }
};

} // beast2
} // boost

#endif
