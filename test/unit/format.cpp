//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/format.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

struct format_test
{
    template<class... Args>
    void f(
        core::string_view match,
        core::string_view fs,
        Args const&... args)
    {
        std::string s;
        format_to(s, fs, args...);
        BOOST_TEST_EQ(s, match);
    }

    template<class... Args>
    void e(
        core::string_view match,
        core::string_view fs,
        Args const&... args)
    {
        BOOST_TEST_THROWS(f(match, fs, args...), std::invalid_argument);
    }

    void run()
    {
        // Bad format strings, string arg.
        e("{}",     "{}");
        e("{",      "{");
        e("}",      "}");
        e("}{",     "}{");
        e("{",      "{",      "x");
        e("}",      "}",      "x");
        e("}{",     "}{",     "x");
        e("{",      "{",      "x");
        e("}",      "}",      "x");
        e("}{",     "}{",     "x");
        e("1{}2{}3","1{}2{}3");
        e("1a2{}3", "1{}2{}3", "a");

        // Good format strings, string arg.
        f("x",      "x");
        f("{",      "{{");
        f("}",      "}}");
        f("}{",     "}}{{");
        f("x",      "x");
        f("x",      "{}",     "x");
        f("x",      "{}",     "x");
        f("{",      "{{",     "x");
        f("}",      "}}",     "x");
        f("}{",     "}}{{",   "x");
        f("1a2b3",  "1{}2{}3", "a", "b");
        f("1a2b3",  "1{}2{}3", "a", "b", "c");
        f("hello world!", "hello {}!", "world");
        f("hello world! {} ", "hello {}! {{}} ", "world");

        // Good format string, char arg
        f("x",      "{}",     'x');
    }
};

TEST_SUITE(format_test, "boost.beast2.format");

} // beast2
} // boost
