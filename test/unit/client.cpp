//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

// Test that header file is self-contained.
#include <boost/http_io/client.hpp>

#include <boost/asio/ip/tcp.hpp>

#include "test_suite.hpp"

namespace boost {
namespace http_io {

struct success_handler
{
    bool pass = false;

    void
    operator()(system::error_code ec, ...)
    {
        pass = BOOST_TEST(! ec.failed());
    }
};

/** Connect two TCP sockets together.
*/
template<class Executor>
bool
connect(
    asio::basic_stream_socket<asio::ip::tcp, Executor>& s1,
    asio::basic_stream_socket<asio::ip::tcp, Executor>& s2)

{
    BOOST_ASSERT(s1.get_executor() == s2.get_executor());
    try
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
        a.async_accept(s2, success_handler());
        s1.async_connect(ep, success_handler());
        s1.get_executor().context().restart();
        s1.get_executor().context().run();
        if(! BOOST_TEST_EQ(s1.remote_endpoint(), s2.local_endpoint()))
            return false;
        if(! BOOST_TEST_EQ(s2.remote_endpoint(), s1.local_endpoint()))
            return false;
    }
    catch(std::exception const&)
    {
        BOOST_TEST_FAIL();
        return false;
    }

    return true;
}

using socket_type =
    asio::basic_stream_socket<
        asio::ip::tcp,
        asio::io_context::executor_type>;

class client_test
{
public:
    void
    testClient()
    {
        asio::io_context ioc;
        socket_type s0(ioc.get_executor());
        socket_type s1(ioc.get_executor());
        connect(s0, s1);
        client<socket_type> s(std::move(s1));
    }

    void
    run()
    {
        testClient();
    }
};

TEST_SUITE(client_test, "boost.http_io.client");

} // http_io
} // boost
