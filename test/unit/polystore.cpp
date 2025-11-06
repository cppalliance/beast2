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
#include <boost/beast2/detail//type_traits.hpp>
#include <boost/core/detail/static_assert.hpp>

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

    void testUnique()
    {
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
            BOOST_TEST_NE(ps.find<D>(), nullptr);
            BOOST_TEST_EQ(ps.get<C>().d, 3.14);
            BOOST_TEST_THROWS(ps.emplace<C>(),
                std::invalid_argument);

            // invoke
            BOOST_TEST_EQ(invoke(ps, &f), 3);
            BOOST_TEST_EQ(invoke(ps, &a0), 1);
            BOOST_TEST_EQ(invoke(ps, &a1), 1);
            BOOST_TEST_EQ(invoke(ps, &a2), 1);
            BOOST_TEST_EQ(invoke(ps, &a3), 1);
            BOOST_TEST_EQ(invoke(ps, &b0), '2');
            BOOST_TEST_EQ(invoke(ps, &b1), '2');
            BOOST_TEST_EQ(invoke(ps, &b2), '2');
            BOOST_TEST_EQ(invoke(ps, &b3), '2');
            BOOST_TEST_EQ(invoke(ps, &c0), 3.14);
        }

        {
            polystore ps;
            {
                auto& d = ps.use<D>();
                d.d = 1.0;
            }
            {
                auto& d = ps.use<D>();
                BOOST_TEST_EQ(d.d, 1.0);
            }

        }
    }

    void testMulti()
    {
        polystore ps;
        auto& a0 = ps.emplace_anon<A>();
        auto& a1 = ps.insert_anon(A{});
        auto& a2 = ps.emplace_anon<A>();
        a0.i = 3;
        a1.i = 4;
        a2.i = 5;
        BOOST_TEST_EQ(a0.i, 3);
        BOOST_TEST_EQ(a1.i, 4);
        BOOST_TEST_EQ(a2.i, 5);
        BOOST_TEST(ps.find<A>() == nullptr);
        BOOST_TEST_THROWS(ps.get<A>(), std::bad_typeid);
    }

    void run()
    {
        testUnique();
        testMulti();
    }
};

TEST_SUITE(polystore_test, "boost.beast2.polystore");

} // beast2
} // boost
