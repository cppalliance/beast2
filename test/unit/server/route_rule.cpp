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

namespace boost {
namespace beast2 {

struct route_rule_test
{
    void check(
        core::string_view pat,
        core::string_view path,
        core::string_view base,
        core::string_view rest)
    {
        auto rv = urls::grammar::parse(pat, path_rule);
        if(! BOOST_TEST(rv.has_value()))
            return;
        route_match m;
        if(! BOOST_TEST(match_route(m, path, *rv, true)))
            return;
        auto base1 = std::string(m.base.buffer());
        auto rest1 = "/" + std::string(m.path.buffer());
        BOOST_TEST_EQ(base1, base);
        BOOST_TEST_EQ(rest1, rest);
    }

    void run()
    {
        //check("/", "/", "/", "/");
        check("/foo", "/foo/bar", "/foo", "/bar");
    }
};

TEST_SUITE(
    route_rule_test,
    "boost.beast2.server.route_rule");

} // beast2
} // boost
