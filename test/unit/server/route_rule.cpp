//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "src/server/route_rule.hpp"

#include "test_suite.hpp"

#if defined(__GNUC__) && __GNUC__ == 12
# pragma GCC diagnostic ignored "-Wrestrict"
#endif

namespace boost {
namespace beast2 {

struct route_rule_test
{
    void run()
    {
    }
};

TEST_SUITE(
    route_rule_test,
    "boost.beast2.server.route_rule");

} // beast2
} // boost
