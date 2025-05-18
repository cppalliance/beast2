//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//

// Test that header file is self-contained.
#include <boost/http_io/write.hpp>

#include "test_suite.hpp"

namespace boost {
namespace http_io {

class any_async_read_stream
{
};

class write_test
{
public:
    void
    testWrite()
    {
    }

    void
    run()
    {
        testWrite();
    }
};

TEST_SUITE(
    write_test,
    "boost.http_io.write");

} // http_io
} // boost
