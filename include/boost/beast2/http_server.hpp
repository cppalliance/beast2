//
// Copyright (c) 2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_HTTP_SERVER_HPP
#define BOOST_BEAST2_HTTP_SERVER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/corosio/tcp_server.hpp>
#include <boost/corosio/io_context.hpp>
#include <cstddef>

namespace boost {
namespace http { class flat_router; }
namespace beast2 {

class BOOST_BEAST2_DECL
    http_server : public corosio::tcp_server
{
    struct impl;
    impl* impl_;

public:
    ~http_server();

    http_server(
        corosio::io_context& ctx,
        std::size_t num_workers,
        http::flat_router router);

private:
    struct worker;

    capy::task<void>
    do_session(worker& w);
};

} // beast2
} // boost

#endif
