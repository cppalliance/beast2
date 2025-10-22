//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/polystore.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

struct polystore_test
{
    struct A
    {
        int i = 1;
    };

    struct B
    {
        char c = '2';
    };

    struct C
    {
        double d;
    };

    struct D : C
    {
        using key_type = C;
        D()
        {
            d = 3.14;
        }
    };

    struct E {};

    static int f()
    {
        return 3;
    }

    static int a0(A const& a)
    {
        return a.i;
    }

    static int a1(A const* a)
    {
        return a->i; 
    }

    static int a2(A& a)
    {
        return a.i;
    }

    static int a3(A* a)
    {
        return a->i;
    }

    static char b0(A const&, B const& b)
    {
        return b.c;
    }

    static char b1(A const*, B const* b)
    {
        return b->c; 
    }

    static char b2(A const&, B& b)
    {
        return b.c;
    }

    static char b3(A const&, B* b)
    {
        return b->c;
    }

    static double c0(C const& c)
    {
        return c.d;
    }

    void run()
    {
        polystore ps;

        BOOST_TEST_EQ(ps.find<A>(), nullptr);
        ps.emplace<A>();
        BOOST_TEST_NE(ps.find<A>(), nullptr);
        BOOST_TEST_EQ(ps.get<A>().i, 1);
        ps.insert(B{});
        BOOST_TEST_NE(ps.find<B>(), nullptr);
        BOOST_TEST_EQ(ps.get<B>().c, '2');
        ps.emplace<D>();
        BOOST_TEST_EQ(ps.find<D>(), nullptr);
        BOOST_TEST_EQ(ps.get<C>().d, 3.14);
        BOOST_TEST_THROWS(ps.emplace<C>(), std::invalid_argument);

        // invoke

        BOOST_TEST_EQ(ps.invoke(&f), 3);
        BOOST_TEST_EQ(ps.invoke(&a0), 1);
        BOOST_TEST_EQ(ps.invoke(&a1), 1);
        BOOST_TEST_EQ(ps.invoke(&a2), 1);
        BOOST_TEST_EQ(ps.invoke(&a3), 1);
        BOOST_TEST_EQ(ps.invoke(&b0), '2');
        BOOST_TEST_EQ(ps.invoke(&b1), '2');
        BOOST_TEST_EQ(ps.invoke(&b2), '2');
        BOOST_TEST_EQ(ps.invoke(&b3), '2');
        BOOST_TEST_EQ(ps.invoke(&c0), 3.14);
    }
};

TEST_SUITE(polystore_test, "boost.beast2.polystore");

} // beast2
} // boost
