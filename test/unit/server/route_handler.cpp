//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/server/route_handler.hpp>

#include <boost/http_proto/request.hpp>
#include <boost/beast2/server/router.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

struct route_handler_test
{
    using test_router = router;

    void check(
        test_router& r,
        http_proto::method verb,
        core::string_view url,
        route_result rv0 = route::send)
    {
        http_proto::request reqm;
        http_proto::request_parser pr;
        Request req(reqm, pr);
        http_proto::response resm;
        http_proto::serializer sr;
        Response res(resm, sr);
        auto rv = r.dispatch(
            verb, urls::url_view(url), req, res);
        if(BOOST_TEST_EQ(rv.message(), rv0.message()))
            BOOST_TEST(rv == rv0);
    }

    void testData()
    {
        static auto const POST = http_proto::method::post;

        struct auth_token
        {
            bool valid = false;
        };

        struct session_token
        {
            bool valid = false;
        };

        test_router r;
        r.use(
            [](Request&, Response& res)
            {
                // create session_token
                auto& st = res.data.try_emplace<session_token>();
                BOOST_TEST_EQ(st.valid, false);
                return route::next;
            });
        r.use("/user",
            [](Request&, Response& res)
            {
                // make session token valid
                auto* st = res.data.find<session_token>();
                if(BOOST_TEST_NE(st, nullptr))
                    st->valid = true;
                return route::next;
            });
        r.route("/user/auth")
            .add(POST,
                [](Request& req, Response& res)
                {
                    auto& st = res.data.get<session_token>();
                    BOOST_TEST_EQ(st.valid, true);
                    // create auth_token each time
                    auto& at = req.data.emplace<auth_token>();
                    at.valid = true;
                    return route::next;
                },
                [](Request& req, Response& res)
                {
                    auto& at = req.data.get<auth_token>();
                    auto& st = res.data.get<session_token>();
                    BOOST_TEST_EQ(at.valid, true);
                    BOOST_TEST_EQ(st.valid, true);
                    return route::send;
                });
        check(r, POST, urls::url_view("/user/auth"));
    }

    void run()
    {
        testData();
    }
};

TEST_SUITE(
    route_handler_test,
    "boost.http_proto.server.route_handler");

} // beast2
} // boost
