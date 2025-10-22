//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/ssl_stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/deferred.hpp>
#include "test_suite.hpp"

namespace boost {
namespace beast2 {

class ssl_stream_test
{
public:
    void
    run()
    {
        asio::io_context ioc;
        asio::ssl::context ctx(asio::ssl::context::tlsv12);
        ssl_stream<asio::ip::tcp::socket> ss(ioc.get_executor(), ctx);
        ss.async_read_some(asio::mutable_buffer{});
    }
};

TEST_SUITE(ssl_stream_test, "boost.beast2.ssl_stream");

} // beast2
} // boost
