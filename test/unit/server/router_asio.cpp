//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/server/router_asio.hpp>

#include <boost/beast2/server/router.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <boost/asio/any_io_executor.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

namespace {

struct Stream
{
    asio::any_io_executor
    get_executor() const
    {
        return {};
    }
};

BOOST_CORE_STATIC_ASSERT(
    std::is_constructible<router, router_asio<Stream>>::value);

} // (anon)

struct router_asio_test
{
    void
    run()
    {
    }
};

TEST_SUITE(router_asio_test, "boost.beast2.server.router_asio");

} // beast2
} // boost