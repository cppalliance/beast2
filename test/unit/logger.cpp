//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/logger.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

struct logger_test
{
    void run()
    {
    }
};

TEST_SUITE(logger_test, "boost.beast2.logger");

} // beast2
} // boost
