//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/server/router.hpp>

#include <boost/beast2/server/route_params.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

struct router_test
{
    struct P
    {
        int n = 0;
    };

    void
    run()
    {
        struct request_t
        {
            http_proto::method method;
            urls::segments_encoded_view path;
        };

        struct response_t
        {
        };

        struct asio_response_t : response_t
        {
        };

        router<asio_response_t, request_t> app;

        {
            router<response_t, request_t> r;

            auto const h = [](request_t&, response_t&) { return true; };
            auto const g = http_proto::method::get;
            r.insert(g, "/path", h);
            r.insert(g, "/path/2", h);
            r.insert(g, "/path/2/3", h);
            r.insert(g, "/path/2/3/", h);
            r.insert(g, "/:x", h);
            r.insert(g, "/*y", h);
            r.insert(g, "/:x(1)", h);
            r.insert(g, "/*z?", h);
            r.insert(g, "/*z+", h);

            r.use(
                [](request_t&, response_t&)
                {
                    return false;
                });
            r.use(
                [](request_t&, response_t&)
                {
                    return true;
                });
            r.insert(http_proto::method::get, "/here",
                [](request_t&, response_t&)
                {
                    return true;
                });

            app.use(
                [](request_t&, asio_response_t&)
                {
                    return true;
                });

            request_t req;
            response_t res;
            req.method = http_proto::method::get;
            req.path = urls::segments_encoded_view("/stuff");


            r(req, res);
            app.insert(http_proto::method::get, "/stuff", r);
            r(req, res);
        }

        request_t req;
        asio_response_t res;
        req.method = http_proto::method::get;
        req.path = urls::segments_encoded_view("/path/to/file.txt");
        app(req, res);
    }
};

TEST_SUITE(
    router_test,
    "boost.beast2.server.router");


} // beast2
} // boost
