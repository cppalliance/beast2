//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/buffers/buffer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/static_assert.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

BOOST_STATIC_ASSERT(
    std::is_constructible<
        asio::const_buffer,
        buffers::const_buffer>::value);

BOOST_STATIC_ASSERT(
    std::is_constructible<
        asio::mutable_buffer,
        buffers::mutable_buffer>::value);

struct buffer_test
{
    void
    run()
    {
    }
};

TEST_SUITE(
    buffer_test,
    "boost.beast2.buffer");

} // beast2
} // boost
