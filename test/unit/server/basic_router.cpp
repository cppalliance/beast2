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
        struct Res : basic_response {};

        BOOST_CORE_STATIC_ASSERT(std::is_copy_assignable<basic_router<Req, Res>>::value);

        struct h0 { void operator()(); };
        struct h1 { system::error_code operator()(); };
        struct h2 { system::error_code operator()(int); };
        struct h3 { system::error_code operator()(Req&, Res&); };
        struct h4 { system::error_code operator()(Req&, Res&, system::error_code); };
        struct h5 { void operator()(Req&, Res&) {} };
        struct h6 { void operator()(Req&, Res&, system::error_code) {} };
        struct h7 { system::error_code operator()(Req&, Res&, int); };
        struct h8 { system::error_code operator()(Req, Res&, int); };
        struct h9 { system::error_code operator()(Req, Res&, system::error_code const&); };

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
        struct Req : basic_request
        {
        };

        struct Res : basic_response
        {
        };

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
        Req req;
        Res res;
        auto ec = r.dispatch(
            http_proto::method::get,
            urls::url_view("/"), req, res);
        BOOST_TEST(ec == route::detach);
        //ec = r.resume(req, res, route::close, st);
        //BOOST_TEST(ec == route::close);
    }

    //--------------------------------------------

    struct Req : basic_request
    {
    };

    struct Res : basic_response
    {
        int i = 0;
    };

    using test_router = basic_router<Req, Res>;

    auto get(
        test_router& r,
        core::string_view url) ->
            route_result
    {
        Req req;
        Res res;
        return r.dispatch(http_proto::method::get,
            urls::url_view(url), req, res);
    }

    struct handler
    {
        ~handler()
        {
            if(alive_)
                BOOST_TEST_EQ(called_, want_ != 0);
        }

        explicit handler(
            int want, system::error_code ec =
                http_proto::error::success)
            : want_(want)
            , ec_(ec)
        {
        }

        handler(handler&& other)
        {
            BOOST_ASSERT(other.alive_);
            BOOST_ASSERT(! other.called_);
            want_ = other.want_;
            alive_ = true;
            other.alive_ = false;
            ec_ = other.ec_;
        }

        route_result operator()(Req&, Res&) const
        {
            called_ = true;
            switch(want_)
            {
            default:
            case 0: return route::close;
            case 1: return route::send;
            case 2: return route::next;
            case 3: return ec_;
            case 4: return route::next_route;
            }
        }

    private:
        // 0 = not called
        // 1 = called
        // 2 = next
        // 3 = error
        // 4 = next_route
        int want_;
        bool alive_ = true;
        bool mutable called_ = false;
        system::error_code ec_;
    };

    struct err_handler
    {
        ~err_handler()
        {
            if(alive_)
                BOOST_TEST_EQ(called_, want_ != 0);
        }

        err_handler(
            int want,
            system::error_code ec)
            : want_(want)
            , ec_(ec)
        {
        }

        err_handler(err_handler&& other)
        {
            BOOST_ASSERT(other.alive_);
            BOOST_ASSERT(! other.called_);
            want_ = other.want_;
            alive_ = true;
            other.alive_ = false;
            ec_ = other.ec_;
        }

        route_result operator()(
            Req&, Res&, system::error_code ec) const
        {
            called_ = true;
            switch(want_)
            {
            default:
            case 0: return route::close;
            case 1:
                BOOST_TEST(ec == ec_);
                return route::send;
            case 2:
                BOOST_TEST(ec == ec_);
                return route::next;
            case 3:
                BOOST_TEST(ec.failed());
                return ec_;
            }
        }

    private:
        // 0 = not called
        // 1 = called, expecting ec_
        // 2 = next, expecting ec_
        // 3 = change error
        int want_;
        bool alive_ = true;
        bool mutable called_ = false;
        system::error_code ec_;
    };

    static handler not_called()
    {
        return handler(0);
    }

    static handler called()
    {
        return handler(1);
    }

    static handler return_next()
    {
        return handler(2);
    }

    static handler return_err(
        system::error_code ec =
            http_proto::error::bad_connection)
    {
        return handler(3, ec);
    }

    static err_handler not_called_err()
    {
        return err_handler(0,
            http_proto::error::success);
    }

    static err_handler send_err(
        system::error_code ec)
    {
        return err_handler(1, ec);
    }

    static err_handler next_err(
        system::error_code ec)
    {
        return err_handler(2, ec);
    }

    static err_handler replace_err(
        system::error_code ec)
    {
        return err_handler(3, ec);
    }

    //--------------------------------------------

    void testUse()
    {
        {
            test_router r;
            r.use(called());
            get(r,"/");
        }
        {
            test_router r;
            r.use("", called());
            get(r,"/");
        }
        {
            test_router r;
            r.use(called());
            r.use(not_called());
            get(r,"/");
        }
        {
            test_router r;
            r.use(
                called(),
                not_called());
            get(r,"/");
        }
        {
            test_router r;
            r.use(called());
            r.use(not_called());
            get(r,"/t");
        }
        {
            test_router r;
            r.use(
                called(),
                not_called());
            get(r,"/t");
        }
        {
            test_router r;
            r.use("/t", not_called());
            r.use("/u", called());
            get(r,"/u");
        }
        {
            test_router r;
            r.use("/t", not_called());
            r.use("/u", called());
            r.use(not_called());
            get(r,"/u");
        }
        {
            test_router r;
            r.use("/x/y", not_called());
            r.use("/x", called());
            get(r,"/x/z");
        }
        {
            test_router r;
            r.use("/x/y", not_called());
            r.use("/x", called());
            get(r,"/x/z");
        }
        {
            test_router r;
            r.use("/x/y", not_called());
            r.use("/x", called());
            r.use(not_called());
            get(r,"/x/z");
        }
        {
            test_router r;
            r.use(return_next());
            r.use(called());
            r.use(not_called());
            get(r,"/");
        }
        {
            test_router r;
            r.use(
                return_next(),
                called(),
                not_called());
            get(r,"/");
        }
        {
            test_router r;
            r.use(return_next());
            r.use(return_err());
            r.use(not_called());
            get(r,"/");
        }
    }

    void testErr()
    {
        system::error_code ec1 =
            http_proto::error::bad_connection;
        system::error_code ec2 =
            http_proto::error::bad_content_length;
        {
            test_router r;
            r.err(not_called_err());
            get(r,"/");
        }
        {
            test_router r;
            r.err("", not_called_err());
            get(r,"/");
        }
        {
            test_router r;
            r.use(called());
            r.use(not_called_err());
            get(r,"/");
        }
        {
            test_router r;
            r.use(return_err(ec1));
            r.use(send_err(ec1));
            r.use(not_called_err());
            get(r,"/");
        }
        {
            test_router r;
            r.use(return_err(ec1));
            r.use("", send_err(ec1));
            r.use(not_called_err());
            get(r,"/");
        }
        {
            test_router r;
            r.use(return_err(ec2));
            r.use(send_err(ec2));
            r.use(not_called_err());
            get(r,"/");
        }

        // mount points
        {
            test_router r;
            r.use("/api", return_err(ec1));
            r.err("/api", send_err(ec1));
            r.err("/x", not_called_err());
            get(r, "/api");
        }
        {
            test_router r;
            r.use("/x", return_err(ec1));
            r.err("/api", not_called_err());
            r.err("/x", send_err(ec1));
            get(r, "/x/data");
        }

        // replacing errors
        {
            test_router r;
            r.use(return_err(ec1));
            r.err(replace_err(ec2));
            r.err(send_err(ec2));
            get(r, "/");
        }

        {
            test_router r;
            r.use(return_err(ec1));
            r.use(not_called());
            r.err(send_err(ec1));
            get(r, "/");
        }

        // route-level vs. router-level
        {
            test_router r;
            r.route("/").get(return_err(ec1));
            r.err(send_err(ec1));
            get(r, "/");
        }

        // subrouters
        {
            test_router api;
            api.use(return_err(ec1));
            api.err(send_err(ec1));

            test_router root;
            root.use("/api", api);
            root.err(not_called_err());
            get(root, "/api");
        }
        {
            test_router api;
            api.use(return_err(ec1));
            api.err(next_err(ec1));

            test_router root;
            root.use("/api", api);
            root.err(send_err(ec1));
            root.err(not_called_err());
            get(root, "/api");
        }
    }

    void testRoute()
    {
        {
            test_router r;
            r.get("/", called());
            get(r,"/");
        }
        {
            test_router r;
            r.get("/x", not_called());
            get(r,"/");
        }
        {
            test_router r;
            r.get("/x", called());
            r.get("/x", not_called());
            get(r,"/x");
        }
        {
            test_router r;
            r.get("/x",
                called(),
                not_called());
            get(r,"/x");
        }
        {
            test_router r;
            r.get("/x", return_next());
            r.get("/x", called());
            r.get("/x", not_called());
            get(r,"/x");
        }
        {
            test_router r;
            r.get("/x",
                return_next(),
                called(),
                not_called());
            get(r,"/x");
        }
    }

    void path(
        core::string_view pat,
        core::string_view target,
        core::string_view good)
    {
        test_router r;
        r.use( pat,
            [&](Req& req, Res&)
            {
                BOOST_TEST_EQ(req.path, good);
                return route::send;
            });
        Req req;
        Res res;
        r.dispatch(
            http_proto::method::get,
            urls::url_view(target),
            req, res);
    }

    void testPath()
    {
        path("/",     "/",       "/");
        path("/",     "/api",    "/api");
        path("/api",  "/api",    "/");
        path("/api",  "/api/",   "/");
        path("/api",  "/api/",   "/");
        path("/api",  "/api/v0", "/v0");
        path("/api/", "/api",    "/");
        path("/api/", "/api",    "/");
        path("/api/", "/api/",   "/");
        path("/api/", "/api/v0", "/v0");
    }

    void run()
    {
        testGrammar();
        testDetach();
        testUse();
        testErr();
        testRoute();
        testPath();
    }
};

TEST_SUITE(
    basic_router_test,
    "boost.beast2.server.basic_router");

} // beast2
} // boost
