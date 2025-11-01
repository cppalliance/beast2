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

namespace {

struct Req {};
struct Res {};

BOOST_CORE_STATIC_ASSERT(std::is_copy_assignable<basic_router<Req, Res>>::value);

} // (anon)

BOOST_CORE_STATIC_ASSERT(grammar::is_charset<unreserved_char>::value);
BOOST_CORE_STATIC_ASSERT(grammar::is_charset<ident_char>::value);
BOOST_CORE_STATIC_ASSERT(grammar::is_charset<constraint_char>::value);

static bool operator==(
    param_segment_rule_t::value_type const& lhs,
    param_segment_rule_t::value_type const& rhs) noexcept
{
    return
        lhs.s == rhs.s &&
        lhs.name == rhs.name &&
        lhs.constraint == rhs.constraint &&
        lhs.prefix == rhs.prefix &&
        lhs.modifier == rhs.modifier;
}

struct basic_router_test
{
    struct h0 { void operator()(); };
    struct h1 { system::error_code operator()(); };
    struct h2 { system::error_code operator()(int); };
    struct h3 { system::error_code operator()(Request&, Response&); };
    struct h4 { system::error_code operator()(Request&, Response&, system::error_code&); };
    struct h5 { void operator()(Request&, Response&) {} };
    struct h6 { void operator()(Request&, Response&, system::error_code) {} };
    struct h7 { system::error_code operator()(Request&, Response&, int); };
    struct h8 { system::error_code operator()(Request, Response&, int); };
    struct h9 { system::error_code operator()(Request, Response&, system::error_code); };

    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h0>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h1>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h2>::value);
    BOOST_CORE_STATIC_ASSERT(  detail::is_handler<h3>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h4>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h5>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h6>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h7>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h8>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_handler<h9>::value);

    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h0>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h1>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h2>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h3>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h4>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h5>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h6>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h7>::value);
    BOOST_CORE_STATIC_ASSERT(! detail::is_error_handler<h8>::value);
    BOOST_CORE_STATIC_ASSERT(  detail::is_error_handler<h9>::value);

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

    void testGrammar()
    {
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
    }

    void testDetach()
    {
        struct Req
        {
            http_proto::method method;
            urls::segments_encoded_view path;
        };
        struct Res
        {
        };

        basic_router<Req, Res> r;
        r.use([](Req&, Res&){ return error::next; });
        {
            basic_router<Req, Res> r1;
            r1.use([](Req&, Res&){ return error::next; });
            r1.use(
                [](Req&, Res&)->system::error_code
                {
                    return error::detach;
                });
            r1.use([](Req&, Res&){ return error::next; });
            r.use(std::move(r1));
        }
        r.use([](Req&, Res&){ return error::next; });
        route_state st;
        Req req;
        Res res;
        req.path = { "/" };
        auto ec = r(req, res, st);
        BOOST_TEST(ec == error::detach);
        ec = r.resume(req, res, error::close, st);
        BOOST_TEST(ec == error::close);
    }

    void run()
    {
        testGrammar();
        testDetach();
    }
};

TEST_SUITE(
    basic_router_test,
    "boost.beast2.server.basic_router");


} // beast2
} // boost
