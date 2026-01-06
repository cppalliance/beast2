//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/server/route_handler_asio.hpp>

#include <boost/http/server/basic_router.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <boost/asio/any_io_executor.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

struct route_handler_asio_test
{
    struct stream
    {
        asio::any_io_executor
        get_executor() const
        {
            return {};
        }
    };

    using test_router = http::basic_router<
        asio_route_params<stream>>;

    void check(
        test_router& r,
        core::string_view url,
        http::route_result rv0 = http::route::send)
    {
        asio_route_params<stream> req;
        auto rv = r.dispatch(
            http::method::get,
            urls::url_view(url), req);
        if(BOOST_TEST_EQ(rv.message(), rv0.message()))
            BOOST_TEST(rv == rv0);
    }

    struct handler
    {
        template<class Stream>
        http::route_result
        operator()(
            asio_route_params<Stream>&) const
        {
            BOOST_TEST(true);
            return http::route::send;
        }
    };

    void run()
    {
        test_router r;
        r.use(handler{});
        check(r,"/");
    }
};

TEST_SUITE(
    route_handler_asio_test,
    "boost.beast2.server.route_handler_asio");

} // beast2
} // boost
