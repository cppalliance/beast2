//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppaliance/beast2
//

#ifndef BOOST_BEAST2_TEST_TCP_HPP
#define BOOST_BEAST2_TEST_TCP_HPP

#include <boost/beast2/detail/config.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/detached.hpp>

namespace boost {
namespace beast2 {
namespace test {

/** Connect two TCP sockets together.
*/
template<class Executor>
bool
connect(
    asio::io_context& ioc,
    asio::basic_stream_socket<asio::ip::tcp, Executor>& s1,
    asio::basic_stream_socket<asio::ip::tcp, Executor>& s2)

{
    asio::basic_socket_acceptor<
        asio::ip::tcp, Executor> a(s1.get_executor());
    auto ep = asio::ip::tcp::endpoint(
        asio::ip::make_address_v4("127.0.0.1"), 0);
    a.open(ep.protocol());
    a.set_option(
        asio::socket_base::reuse_address(true));
    a.bind(ep);
    a.listen(0);
    ep = a.local_endpoint();
    a.async_accept(s2, asio::detached);
    s1.async_connect(ep, asio::detached);
    ioc.run();
    ioc.restart();
    if(! s1.remote_endpoint() == s2.local_endpoint())
        return false;
    if(! s2.remote_endpoint() == s1.local_endpoint())
        return false;
    return true;
}

} // test
} // beast2
} // boost

#endif
