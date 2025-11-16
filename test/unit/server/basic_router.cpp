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

#include "src/server/route_rule.hpp"

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

struct basic_router_test
{
    void compileTimeTests()
    {
        struct Req : basic_request {};
        struct Res : basic_response {};
        struct OtherReq : basic_request {};

        BOOST_CORE_STATIC_ASSERT(std::is_copy_assignable<basic_router<Req, Res>>::value);

        struct h0 { void operator()(); };
        struct h1 { system::error_code operator()(); };
        struct h2 { system::error_code operator()(int); };
        struct h3 { system::error_code operator()(Req&, Res&) const; };
        struct h4 { system::error_code operator()(Req&, Res&, system::error_code) const; };
        struct h5 { void operator()(Req&, Res&) {} };
        struct h6 { void operator()(Req&, Res&, system::error_code) {} };
        struct h7 { system::error_code operator()(Req&, Res&, int); };
        struct h8 { system::error_code operator()(Req, Res&, int); };
        struct h9 { system::error_code operator()(Req, Res&, system::error_code const&) const; };

#if 0
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h0, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h1, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h2, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h3, Req, Res>::value == 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h4, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h5, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h6, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h7, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h8, Req, Res>::value != 1);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h9, Req, Res>::value != 1);

        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h0, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h1, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h2, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h3, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h4, Req, Res>::value == 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h5, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h6, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h7, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h8, Req, Res>::value != 2);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<h9, Req, Res>::value == 2);

        BOOST_CORE_STATIC_ASSERT(detail::handler_type<
            basic_router<Req, Res>, Req, Res>::value == 4);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<
            basic_router<basic_request, Res>, Req, Res>::value == 4);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<
            basic_router<Req, basic_response>, Req, Res>::value == 4);
        BOOST_CORE_STATIC_ASSERT(detail::handler_type<
            basic_router<OtherReq, Res>, Req, Res>::value == 0);
#endif
    }

    //--------------------------------------------

    using Req = basic_request;
    using Res = basic_response;

    /** A handler for testing
    */
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

    /** An error handler for testing
    */
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

    // handler to check base_url and path
    struct path
    {
        ~path()
        {
            if(alive_)
                BOOST_TEST(called_);
        }
        path(path&& other)
        {
            BOOST_ASSERT(other.alive_);
            BOOST_ASSERT(! other.called_);
            alive_ = true;
            other.alive_ = false;
            base_path_ = other.base_path_;
            path_ = other.path_;
        }
        path(
            core::string_view path = "/")
            : path_(path)
        {
        }
        path(
            core::string_view base_path,
            core::string_view path)
            : base_path_(base_path)
            , path_(path)
        {
        }
        route_result operator()(Req& req, Res&) const
        {
            called_ = true;
            BOOST_TEST_EQ(req.base_path, base_path_);
            BOOST_TEST_EQ(req.path, path_);
            return route::next;
        }
    private:
        bool alive_ = true;
        bool mutable called_ = false;
        core::string_view base_path_;
        core::string_view path_;
    };

    //-------------------------------------------

    // must NOT be called
    static handler skip()
    {
        return handler(0);
    }

    // must be called
    static handler send()
    {
        return handler(1);
    }

    // must be called, returns route::next
    static handler next()
    {
        return handler(2);
    }

    // must be called, returns ec
    static handler fail(
        system::error_code ec)
    {
        return handler(3, ec);
    }

    // must NOT be called
    static err_handler err_skip()
    {
        return err_handler(0,
            http_proto::error::success);
    }

    // must be called, expects ec, returns route::send
    static err_handler err_send(
        system::error_code ec)
    {
        return err_handler(1, ec);
    }

    // must be called with `ec`, returns route::next
    static err_handler err_next(
        system::error_code ec)
    {
        return err_handler(2, ec);
    }

    // must be called, returns a new error `ec`
    static err_handler err_return(
        system::error_code ec)
    {
        return err_handler(3, ec);
    }

    using test_router = basic_router<Req, Res>;

    void check(
        test_router& r,
        core::string_view url,
        route_result rv0 = route::send)
    {
        Req req;
        Res res;
        auto rv = r.dispatch(
            http_proto::method::get,
            urls::url_view(url), req, res);
        if(BOOST_TEST_EQ(rv.message(), rv0.message()))
            BOOST_TEST(rv == rv0);
    }

    void check(
        test_router& r,
        http_proto::method verb,
        core::string_view url,
        route_result rv0 = route::send)
    {
        Req req;
        Res res;
        auto rv = r.dispatch(verb,
            urls::url_view(url), req, res);
        if(BOOST_TEST_EQ(rv.message(), rv0.message()))
            BOOST_TEST(rv == rv0);
    }

    void check(
        test_router& r,
        core::string_view verb,
        core::string_view url,
        route_result rv0 = route::send)
    {
        Req req;
        Res res;
        auto rv = r.dispatch(verb,
            urls::url_view(url), req, res);
        if(BOOST_TEST_EQ(rv.message(), rv0.message()))
            BOOST_TEST(rv == rv0);
    }

    //--------------------------------------------

    void testEmpty()
    {
        // routers with no layers
        test_router r;
        check(r, "/", route::next);
    }

    void testUse()
    {
        system::error_code const er =
            http_proto::error::bad_connection;
        system::error_code const er2 =
            http_proto::error::bad_expect;

        // pathless
        {
            test_router r;
            r.use(send());
            check(r,"/");
        }
        {
            test_router r;
            r.use(
                path(),
                send());
            check(r,"/");
        }
        {
            test_router r;
            r.use(
                send(),
                skip());
            check(r,"/");
        }
        {
            test_router r;
            r.use(send());
            r.use(skip());
            check(r,"/");
        }
        {
            test_router r;
            r.use(
                next(),
                send());
            check(r,"/");
        }
        {
            test_router r;
            r.use(next());
            r.use(send());
            check(r,"/");
        }
        {
            test_router r;
            r.use(next());
            r.use(send());
            r.use(skip());
            check(r,"/");
        }
        {
            test_router r;
            r.use(
                next(),
                send(),
                skip());
            check(r,"/");
        }

        // pathless with errors
        {
            test_router r;
            r.use(fail(er));
            check(r, "/", er);
        }
        {
            test_router r;
            r.use(next());
            r.use(err_skip());
            r.use(fail(er));
            r.use(skip());
            r.use(err_send(er));
            r.use(skip());
            r.use(err_skip());
            check(r,"/");
        }
        {
            test_router r;
            r.use(
                next(),
                err_skip(),
                fail(er),
                skip(),
                err_send(er),
                skip(),
                err_skip());
            check(r,"/");
        }
        {
            test_router r;
            r.use(next());
            r.use(err_skip());
            r.use(fail(er));
            r.use(skip());
            r.use(err_return(er2));
            r.use(skip());
            r.use(err_next(er2));
            r.use(err_send(er2));
            check(r,"/");
        }
        {
            test_router r;
            r.use(
                next(),
                err_skip(),
                fail(er),
                skip(),
                err_return(er2),
                skip(),
                err_next(er2),
                err_send(er2));
            check(r,"/");
        }
        {
            // cannot return success
            test_router r;
            r.use(fail(system::error_code()));
            BOOST_TEST_THROWS(check(r,"/"),
                std::invalid_argument);
        }
        {
            // can't change failure to success
            test_router r;
            r.use(
                fail(er),
                err_return(system::error_code{}));
            BOOST_TEST_THROWS(check(r,"/"),
                std::invalid_argument);
        }

        // pathless, returning route enums
        {
            test_router r;
            r.use(fail(route::close));
            check(r,"/", route::close);
        }
        {
            test_router r;
            r.use(fail(route::complete));
            check(r,"/", route::complete);
        }
        {
            test_router r;
            r.use(fail(route::detach));
            check(r,"/", route::detach);
        }
        {
            test_router r;
            r.use(fail(route::next));
            check(r,"/", route::next);
        }
        {
            // middleware can't return route::next_route
            test_router r;
            r.use(fail(route::next_route));
            BOOST_TEST_THROWS(check(r,"/", route::next),
                std::invalid_argument);
        }
        {
            test_router r;
            r.use(fail(route::send));
            check(r,"/", route::send);
        }

        // empty path
        {
            test_router r;
            r.use("", send());
            check(r,"/");
            check(r,"/api");
        }
        {
            test_router r;
            r.use("",
                path("/api"),
                send());
            check(r,"/api");
        }

        // prefix matching
        {
            test_router r;
            r.use("/api", skip());
            check(r,"/", route::next);
        }
        {
            test_router r;
            r.use("/api", skip());
            check(r,"/", route::next);
            check(r,"/a", route::next);
            check(r,"/ap", route::next);
        }
        {
            test_router r;
            r.use("/api", send());
            check(r,"/api");
            check(r,"/api/");
            check(r,"/api/more");
        }
        {
            test_router r;
            r.use("/api",
                path("/api", "/more"),
                send());
            check(r,"/api/more");
        }
        {
            test_router r;
            r.use("/api/more",
                path("/api/more", "/"),
                send());
            check(r,"/api/more");
        }
        {
            test_router r;
            r.use("/api", next());
            r.use("/api", send());
            check(r,"/api");
            check(r,"/api/");
            check(r,"/api/more");
        }
        {
            test_router r;
            r.use("/api",
                next(),
                send());
            check(r,"/api");
            check(r,"/api/");
            check(r,"/api/more");
        }
        {
            test_router r;
            r.use("/api",
                next(),
                send(),
                err_skip(),
                skip());
            check(r,"/api");
            check(r,"/api/");
            check(r,"/api/more");
        }
        {
            test_router r;
            r.use("/api", skip());
            r.use("/", send());
            check(r,"/");
        }
        {
            test_router r;
            r.use("/", next());
            r.use("/api", skip());
            r.use("/", send());
            check(r,"/");
        }
        {
            test_router r;
            r.use("/x", next());
            r.use("/api", skip());
            r.use("/y", skip());
            r.use("/x", send());
            check(r,"/x");
        }
        {
            // no match
            test_router r;
            r.use("/x", skip());
            r.use("/api", skip());
            r.use("/y", skip());
            r.use("/x", skip());
            check(r,"/", route::next);
        }

        // errors and matching
        {
            test_router r;
            r.use("/x", skip());
            r.use("/api", skip());
            r.use("/y", fail(er));
            r.use("/y", skip());
            r.use("/x", err_skip());
            r.use("/y", err_return(er2));
            r.use("/z/", err_skip());
            r.use("/y", err_next(er2));
            r.use("/y", err_send(er2));
            r.use("/y", err_skip());
            r.use("/y", skip());
            check(r,"/y");
        }
        {
            test_router r;
            r.use("/x", skip());
            r.use("/api", skip());
            r.use("/y",
                fail(er),
                skip());
            r.use("/x", err_skip());
            r.use("/y", err_return(er2));
            r.use("/z/", err_skip());
            r.use("/y",
                err_next(er2),
                err_send(er2),
                err_skip(),
                skip());
            check(r,"/y");
        }
    }

    void testRoute()
    {
        static auto const GET = http_proto::method::get;
        static auto const POST = http_proto::method::post;
        static system::error_code const er =
            http_proto::error::bad_connection;
        static system::error_code const er2 =
            http_proto::error::bad_expect;

        // empty
        {
            test_router r;
            check(r, "/", route::next);
            check(r, GET, "/", route::next);
            check(r, POST, "/", route::next);
            check(r, "GET", "/", route::next);
            check(r, "POST", "/", route::next);
            BOOST_TEST_THROWS(
                check(r, "", "/", route::next),
                std::invalid_argument);
        }

        // add
        {
            test_router r;
            r.add(GET, "/",
                path(),
                send());
            check(r, GET, "/");
            check(r, "GET", "/");
            check(r, "get", "/", route::next);
            check(r, POST, "/", route::next);
            check(r, "POST", "/", route::next);
            check(r, "post", "/", route::next);
        }
        {
            test_router r;
            r.add(POST, "/", send());
            check(r, POST, "/");
            check(r, "POST", "/");
            check(r, "Post", "/", route::next);
            check(r, GET, "/", route::next);
            check(r, "GET", "/", route::next);
            check(r, "get", "/", route::next);
        }
        {
            test_router r;
            r.add(GET, "/x", skip());
            r.add(POST, "/y", skip());
            r.add(GET, "/y",
                path("/y", "/"),
                send());
            r.add(GET, "/z", skip());
            check(r, GET, "/y");
        }
        {
            test_router r;
            r.add("HACK", "/", next());
            r.add("CRACK", "/", send());
            r.add(GET, "/", skip());
            check(r, "CRACK", "/");
            check(r, "crack", "/", route::next);
            check(r, "HACK", "/", route::next);
        }

        // route.add
        {
            test_router r;
            r.route("/")
                .add(GET, send());
            check(r, GET, "/");
            check(r, "GET", "/");
            check(r, "get", "/", route::next);
            check(r, POST, "/", route::next);
            check(r, "POST", "/", route::next);
            check(r, "post", "/", route::next);
        }
        {
            test_router r;
            r.route("/")
                .add(POST, send());
            check(r, POST, "/");
            check(r, "POST", "/");
            check(r, "Post", "/", route::next);
            check(r, GET, "/", route::next);
            check(r, "GET", "/", route::next);
            check(r, "get", "/", route::next);
        }
        {
            test_router r;
            r.route("/x").add(GET, skip());
            r.route("/y")
                .add(POST, skip())
                .add(GET, send());
            r.route("/z")
                .add(GET, skip());
            check(r, GET, "/y");
        }
        {
            test_router r;
            r.route("/")
                .add("HACK", next())
                .add("CRACK", send())
                .add(GET, skip());
            check(r, "CRACK", "/");
            check(r, "crack", "/", route::next);
            check(r, "HACK", "/", route::next);
        }

        // mix with use
        {
            test_router r;
            r.use(next());
            r.add(POST, "/x", skip());
            r.add(POST, "/y", skip());
            r.use("/z", next());
            r.add(POST, "/y", skip());
            r.add(POST, "/z", send());
            r.add(POST, "/z", skip());
            r.use(skip());
            check(r, POST, "/z");
        }

        // verb matching
        {
            test_router r;
            r.add(GET, "/", send());
            check(r, GET, "/");
            check(r, POST, "/", route::next);
            check(r, "GET", "/");
            check(r, "POST", "/", route::next);
            check(r, "get", "/", route::next);
            check(r, "Get", "/", route::next);
            check(r, "gEt", "/", route::next);
            check(r, "geT", "/", route::next);
            check(r, "post", "/", route::next);
        }
        {
            test_router r;
            r.route("/")
                .add(POST, skip())
                .add(GET, send())
                .add(GET, skip())
                .add(POST, skip());
            check(r, GET, "/");
        }
        {
            test_router r;
            r.route("/")
                .add(GET, skip())
                .add(POST, send())
                .add(POST, skip())
                .add(GET, skip());
            check(r, POST, "/");
        }

        // all
        {
            test_router r;
            r.all("/x", skip());
            r.all("/y", send());
            r.all("/z", skip());
            check(r, GET, "/y");
        }
        {
            test_router r;
            r.all("/y", next());
            r.all("/y", send());
            r.all("/z", skip());
            check(r, GET, "/y");
        }
        {
            test_router r;
            r.add(GET, "/y", next());
            r.all("/y", send());
            r.all("/z", skip());
            check(r, GET, "/y");
        }
        {
            test_router r;
            r.add(POST, "/y", skip());
            r.all("/y", send());
            r.all("/z", skip());
            check(r, GET, "/y");
        }
        {
            test_router r;
            r.add(GET, "/x", skip());
            r.all("/y", send());
            r.use("/z", skip());
            check(r, GET, "/y");
        }
        {
            test_router r;
            BOOST_TEST_THROWS(
                r.all("", skip()),
                std::invalid_argument);
        }

        // error handling
        {
            test_router r;
            r.use(err_skip());
            r.route("/")
                .add(GET, skip())
                .add(POST, skip())
                .add("FAIL", fail(er))
                .add("HEAD", skip());
            check(r, "FAIL", "/", er);
        }
        {
            test_router r;
            r.use(err_skip());
            r.route("/")
                .add(GET, skip())
                .add(POST, skip())
                .add("FAIL", fail(er))
                .add("HEAD", skip());
            r.use(
                err_send(er),
                err_skip());
            check(r, "FAIL", "/");
        }
        {
            test_router r;
            r.route("/")
                .add(GET, skip())
                .add(POST, fail(er))
                .add(POST, skip());
            r.use(
                err_return(er2),
                err_next(er2));
            r.use(
                err_send(er2));
            check(r, POST, "/");
        }
    }

    void testSubRouter()
    {
        static auto const GET = http_proto::method::get;
        static auto const POST = http_proto::method::post;
        static system::error_code const er =
            http_proto::error::bad_connection;
        static system::error_code const er2 =
            http_proto::error::bad_expect;

        // sub-middleware
        {
            test_router r;
            r.use("/api", []{
                test_router r;
                r.use("/v1",
                    skip());
                r.use("/v2",
                    path("/api/v2", "/"),
                    send());
                return r; }());
            check(r,"/api/v2");
        }

        // error handling
        {
            test_router r;
            r.use("/api", []{
                test_router r;
                r.use("/v1",
                    path("/api/v1", "/"),
                    fail(er)); // return er
                return r; }());
            check(r,"/api/v1", er);
        }
        {
            test_router r;
            r.use("/api", []{
                test_router r;
                r.use("/v1",
                    path("/api/v1", "/"),
                    fail(er));
                return r; }());
            r.use(err_next(er)); // next
            check(r,"/api/v1", er);
        }
        {
            test_router r;
            r.use("/api", []{
                test_router r;
                r.use("/v1",
                    path("/api/v1", "/"),
                    fail(er));
                return r; }());
            r.use(
                skip());
            r.use([]{
                test_router r;
                r.use(
                    skip(),
                    err_skip());
                return r; }());
            r.use("/api/v2",
                err_skip());
            r.use(
                err_next(er));
            check(r,"/api/v1", er);
        }
        {
            test_router r;
            r.use([]{
                test_router r;
                r.use([]{
                    test_router r;
                    r.use(fail(er));
                    return r; }());
                r.use(
                    err_next(er),
                    skip());
                return r; }());
            r.use(
                err_return(er2),
                err_next(er2),
                err_return(er));
            check(r, "/", er);
        }

        // sub routes
        {
            test_router r;
            r.use("/api", []{
                test_router r;
                r.route("/user")
                    .add(POST, path("/api/user", "/"))
                    .add(GET, skip())
                    .add(POST, send());
                return r; }());
            check(r, POST, "/api/user");
        }

    }

    void testErr()
    {
        static auto const GET = http_proto::method::get;
        static system::error_code const er =
            http_proto::error::bad_connection;
        static system::error_code const er2 =
            http_proto::error::bad_content_length;
        {
            test_router r;
            r.use(err_skip());
            check(r,"/", route::next);
        }
        {
            test_router r;
            r.use("", err_skip());
            check(r,"/", route::next);
        }
        {
            test_router r;
            r.use(send());
            r.use(err_skip());
            check(r,"/");
        }
        {
            test_router r;
            r.use(fail(er));
            r.use(err_send(er));
            r.use(err_skip());
            check(r,"/", route::send);
        }
        {
            test_router r;
            r.use(fail(er));
            r.use("", err_send(er));
            r.use(err_skip());
            check(r,"/", route::send);
        }
        {
            test_router r;
            r.use(fail(er2));
            r.use(err_send(er2));
            r.use(err_skip());
            check(r,"/", route::send);
        }

        // mount points
        {
            test_router r;
            r.use("/api", fail(er));
            r.use("/api", err_send(er));
            r.use("/x", err_skip());
            check(r, "/api");
        }
        {
            test_router r;
            r.use("/x", fail(er));
            r.use("/api", err_skip());
            r.use("/x", err_send(er));
            check(r, "/x/data");
        }

        // replacing errors
        {
            test_router r;
            r.use(fail(er));
            r.use(err_return(er2));
            r.use(err_send(er2));
            check(r, "/");
        }

        {
            test_router r;
            r.use(fail(er));
            r.use(skip());
            r.use(err_send(er));
            check(r, "/");
        }

        // route-level vs. router-level
        {
            test_router r;
            r.route("/").add(GET, fail(er));
            r.use(err_send(er));
            check(r, "/");
        }

        // subrouters
        {
            test_router r;
            r.use("/api", []{
                test_router r;
                r.use(
                    fail(er),
                    err_send(er),
                    err_skip());
                return r; }());
            r.use(err_skip());
            check(r, "/api");
        }
        {
            test_router r;
            r.use("/api", []{
                test_router r;
                r.use(
                    fail(er),
                    err_next(er),
                    skip());
                return r; }());
            r.use(err_send(er));
            r.use(err_skip());
            check(r, "/api");
        }
    }

    void testPath()
    {
        auto const path = [](
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
        };

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

    void testDetach()
    {
        static auto const GET = http_proto::method::get;
        {
            test_router r;
            r.use(next());
            r.use(fail(route::detach));
            check(r,"/", route::detach);
        }
        {
            test_router r;
            r.use(next());
            r.use(fail(route::detach));
            Req req;
            Res res;
            auto rv1 = r.dispatch(GET, urls::url_view("/"), req, res);
            BOOST_TEST(rv1 == route::detach);
        }
        {
            test_router r;
            r.use(next());
            r.use(fail(route::detach));
            r.use(next());
            r.use(fail(route::send));
            Req req;
            Res res;
            auto rv1 = r.dispatch(GET, urls::url_view("/"), req, res);
            BOOST_TEST(rv1 == route::detach);
            auto rv2 = r.resume(req, res, route::next);
            BOOST_TEST(rv2 == route::send);
        }
        {
            test_router r;
            r.use(next());
            r.use([]{
                test_router r;
                r.use(
                    next(),
                    fail(route::detach),
                    path(),
                    next());
                return r; }());
            r.use(send());
            Req req;
            Res res;
            auto rv1 = r.dispatch(GET, urls::url_view("/"), req, res);
            BOOST_TEST(rv1 == route::detach);
            auto rv2 = r.resume(req, res, route::next);
            BOOST_TEST(rv2 == route::send);
        }
    }

    void run()
    {
        testEmpty();
        testUse();
        testRoute();
        testSubRouter();
        testErr();
        testPath();
        testDetach();
    }
};

TEST_SUITE(
    basic_router_test,
    "boost.http_proto.server.basic_router");

} // beast2
} // boost
