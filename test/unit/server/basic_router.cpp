//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/server/basic_router.hpp>

#include <boost/beast2/server/route_handler.hpp>
#include <boost/beast2/error.hpp>

#include "src/server/route_rule.hpp"

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

BOOST_CORE_STATIC_ASSERT(grammar::is_charset<unreserved_char>::value);
BOOST_CORE_STATIC_ASSERT(grammar::is_charset<ident_char>::value);
BOOST_CORE_STATIC_ASSERT(grammar::is_charset<constraint_char>::value);

struct basic_router_test
{
    void compileTimeTests()
    {
        struct Req {};
        struct Res {};

        BOOST_CORE_STATIC_ASSERT(std::is_copy_assignable<basic_router<Req, Res>>::value);

        struct h0 { void operator()(); };
        struct h1 { system::error_code operator()(); };
        struct h2 { system::error_code operator()(int); };
        struct h3 { system::error_code operator()(Req&, Res&); };
        struct h4 { system::error_code operator()(Req&, Res&, system::error_code&); };
        struct h5 { void operator()(Req&, Res&) {} };
        struct h6 { void operator()(Req&, Res&, system::error_code) {} };
        struct h7 { system::error_code operator()(Req&, Res&, int); };
        struct h8 { system::error_code operator()(Req, Res&, int); };
        struct h9 { system::error_code operator()(Req, Res&, system::error_code); };

        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h0, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h1, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h2, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h3, Req, Res>::value == 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h4, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h5, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h6, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h7, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h8, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h9, Req, Res>::value != 1);

        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h0, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h1, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h2, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h3, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h4, Req, Res>::value == 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h5, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h6, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h7, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h8, Req, Res>::value != 3);
        BOOST_CORE_STATIC_ASSERT(detail::get_handler_kind<h9, Req, Res>::value == 3);
    }

#if 0
    template<class T>
    static void good(core::string_view s, T const& t)
    {
        auto rv = grammar::parse(s, t);
        if(! BOOST_TEST_EQ(rv.has_value(), true))
            return;
        BOOST_TEST_EQ(rv.value(), s);
    };

    template<class T>
    static void good(core::string_view s,
        typename T::value_type match, T const& t)
    {
        auto rv = grammar::parse(s, t);
        if(! BOOST_TEST_EQ(rv.has_value(), true))
            return;
        BOOST_TEST_EQ(rv.value(), match);
    };

    template<class T>
    static void bad(
        core::string_view s, T const& t,
        system::error_code const& ec = grammar::error::syntax)
    {
        auto rv = grammar::parse(s, t);
        if(! BOOST_TEST_EQ(rv.has_error(), true))
            return;
        BOOST_TEST_EQ(rv.error(), ec);
    };

    static void lit(core::string_view s)
    {
        auto rv = grammar::parse(s, segment_rule);
        if(! BOOST_TEST_EQ(rv.has_value(), true))
            return;
        auto const& v = rv.value();
        BOOST_TEST_EQ(v.s, s);
        BOOST_TEST_EQ(v.name, "");
        BOOST_TEST_EQ(v.constraint, "");
        BOOST_TEST_EQ(v.prefix, 0);
        BOOST_TEST_EQ(v.modifier, 0);
    };

    static std::string to_string(
        path_rule_t::value_type const& pat)
    {
        std::string s;
        if(pat.v.empty())
            return "/";
        for(auto it = pat.v.begin();
            it != pat.v.end();)
        {
            s.push_back('/');
            auto const& v = *it++;
            if(v.s.empty() && v.name.empty())
            {
                // trailing "/"
                BOOST_TEST_EQ(it, pat.v.end());
                break;
            }
            if(! v.s.empty())
            {
                // literal-segment
                BOOST_TEST(v.name.empty());
                s.append(v.s);
                continue;
            }
            s.push_back(v.prefix);
            s.append(v.name);
            if(! v.constraint.empty())
            {
                s.push_back('(');
                s.append(v.constraint);
                s.push_back(')');
            }
            if(v.modifier)
                s.push_back(v.modifier);
        }
        return s;
    }

    static void path(core::string_view s)
    {
        auto rv = grammar::parse(s, path_rule);
        if(! BOOST_TEST(rv.has_value()))
            return;
        BOOST_TEST_EQ(s, to_string(*rv));
    }

    param_segment_rule_t::value_type vt(
        core::string_view s,
        core::string_view name,
        core::string_view constraint,
        char prefix,
        char modifier)
    {
        param_segment_rule_t::value_type v;
        v.s = s;
        v.name = name;
        v.constraint = constraint;
        v.prefix = prefix;
        v.modifier = modifier;
        return v;
    }
#endif

    void testGrammar()
    {
#if 0
        // constraint_rule
        good("(a)",  "a",   constraint_rule);
        good("(ab)", "ab",  constraint_rule);
        good("(a1)", "a1",  constraint_rule);
        good("(a )", "a ",  constraint_rule);
        good("(a b)","a b", constraint_rule);
        good("", "",        constraint_rule);
        bad ("{",           constraint_rule, grammar::error::leftover);         
        bad ("(",           constraint_rule);
        bad ("()",          constraint_rule);

        // param_name_rule
        good("a",   param_name_rule);
        good("a1",  param_name_rule);
        good("ab",  param_name_rule);
        good("a_",  param_name_rule);
        good("a1_", param_name_rule);
        good("a1_b",param_name_rule);
        bad ("",    param_name_rule, grammar::error::syntax);
        bad ("1",   param_name_rule);
        bad ("a$",  param_name_rule, grammar::error::leftover);

        // param-segment
        good(":id",     vt("", "id", "", ':', 0), param_segment_rule);
        good(":id(1)",  vt("", "id", "1", ':', 0), param_segment_rule);
        good(":id?",    vt("", "id", "", ':', '?'), param_segment_rule);
        good(":id(x)+", vt("", "id", "x", ':', '+'), param_segment_rule);
        good("*x",      vt("", "x", "", '*', 0), param_segment_rule);
        bad ("?",       param_segment_rule, grammar::error::mismatch);
        bad (":",       param_segment_rule);
        bad (":0",      param_segment_rule);
        bad (":a(",     param_segment_rule);
        bad (":(",      param_segment_rule);
        bad (":a()",    param_segment_rule);

        // literal-segment
        good("a",       literal_segment_rule);
        good("ab",      literal_segment_rule);
        good("a1",      literal_segment_rule);
        good("a1b",     literal_segment_rule);
        bad ("",        literal_segment_rule, grammar::error::syntax);
        bad ("/",       literal_segment_rule, grammar::error::mismatch);
        bad (" ",       literal_segment_rule, grammar::error::mismatch);

        // segment
        good("a",       vt("a", "", "", 0, 0), segment_rule);
        good("ab",      vt("ab", "", "", 0, 0), segment_rule);
        good("a1",      vt("a1", "", "", 0, 0), segment_rule);
        good("a1b",     vt("a1b", "", "", 0, 0), segment_rule);
        good(":id",     vt("", "id", "", ':', 0), segment_rule);
        good(":id(1)",  vt("", "id", "1", ':', 0), segment_rule);
        good(":id?",    vt("", "id", "", ':', '?'), segment_rule);
        good(":id(x)+", vt("", "id", "x", ':', '+'), segment_rule);
        good("*x",      vt("", "x", "", '*', 0), segment_rule);
        good("?",       vt("?", "", "", 0, 0), segment_rule);
        bad ("",        segment_rule, grammar::error::syntax);
        bad ("/",       segment_rule, grammar::error::mismatch);
        bad (" ",       segment_rule, grammar::error::mismatch);
        bad (":",       segment_rule);
        bad (":0",      segment_rule);
        bad (":a(",     segment_rule);
        bad (":(",      segment_rule);
        bad (":a()",    segment_rule);

        // path
        path("/");
        path("/a");
        path("/a/");
        path("/a/b");
        path("/a:id/b");
#endif
    }

    void testDetach()
    {
#if 0
        basic_router<Req, Res> r;
        r.use([](Req&, Res&){ return route::next; });
        {
            basic_router<Req, Res> r1;
            r1.use([](Req&, Res&){ return route::next; });
            r1.use(
                [](Req&, Res&) -> route_result
                {
                    return route::detach;
                });
            r1.use([](Req&, Res&){ return route::next; });
            r.use(std::move(r1));
        }
        r.use([](Req&, Res&){ return route::next; });
        route_state st;
        Req req;
        Res res;
        req.path = "/";
        auto ec = r(req, res, st);
        BOOST_TEST(ec == route::detach);
        ec = r.resume(req, res, route::close, st);
        BOOST_TEST(ec == route::close);
#endif
    }

    //--------------------------------------------

    struct Req
    {
        core::string_view base_path;
        core::string_view path;
    };

    struct Res
    {
    };

    using test_router = basic_router<Req, Res>;

    class h_err
    {
        route_result ec_;

    public:
        h_err(route_result ec) noexcept
            : ec_(ec)
        {
        }

        route_result operator()(Req&, Res&) const
        {
            return ec_;
        }
    };

    // ensures a 1-route router matches the pattern
    void good1(
        core::string_view path,
        core::string_view pat)
    {
        system::error_code const ev =
            http_proto::error::bad_connection;
        test_router r;
        r.use(pat, [ev](Req&, Res&) { return ev; });
        Req req{ "", path };
        Res res;
        route_state st;
        auto rv = r.dispatch(http_proto::method::get,
            urls::url_view(path), req, res, st);
        BOOST_TEST(rv == ev);
    }

    void bad1(
        core::string_view path,
        core::string_view pat)
    {
        system::error_code const ev =
            http_proto::error::bad_connection;
        test_router r;
        r.use(pat, [ev](Req&, Res&) { return ev; });
        Req req{ "", path };
        Res res;
        route_state st;
        auto rv = r.dispatch(http_proto::method::get,
            urls::url_view(path), req, res, st);
        BOOST_TEST(rv == route::next);
    }

    void testUse()
    {
        good1("/",    "/");
        good1("/x",   "/x");
        good1("/xy",  "/xy");
        good1("/xy",  "/x");
        good1("/x/y", "/x");
        route_result nr[] = {
            http_proto::error::bad_connection,
            http_proto::error::bad_content_length,
            http_proto::error::bad_field_name
        };
        {
            test_router r;
            r.use( h_err(nr[0]) );
            Req req{ "", "/" };
            Res res;
            route_state st;
            auto rv = r.dispatch(http_proto::method::get,
                urls::url_view("/"), req, res, st);
            BOOST_TEST(rv == nr[0]);
        }
        {
            test_router r;
            r.use( "/a", h_err(nr[0]) );
            r.use( "/b", h_err(nr[1]) );
            r.use( "/c", h_err(nr[2]) );
            Req req{ "", "/b" };
            Res res;
            route_state st;
            auto rv = r.dispatch(http_proto::method::get,
                urls::url_view("/b"), req, res, st);
            BOOST_TEST(rv == nr[1]);
        }
    }

    void run()
    {
        testGrammar();
        testDetach();
        testUse();
    }
};

TEST_SUITE(
    basic_router_test,
    "boost.beast2.server.basic_router");


} // beast2
} // boost
