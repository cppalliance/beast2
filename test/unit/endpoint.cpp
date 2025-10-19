//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

// Test that header file is self-contained.
#include <boost/http_io/endpoint.hpp>

#include "test_suite.hpp"

namespace boost {
namespace http_io {

struct endpoint_test
{
    void
    run()
    {
    }
};

TEST_SUITE(
    endpoint_test,
    "boost.http_io.endpoint");

} // http_io
} // boost
