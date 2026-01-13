//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
//#include <boost/beast2/stream.hpp>

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/test/stream.hpp>
#include <boost/capy/buffers/any_stream.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/capy/task.hpp>

#include "test_suite.hpp"

#ifdef BOOST_BEAST_HAS_CORO

namespace boost {
namespace beast2 {

template<class AsyncStream>
auto make_stream(
    AsyncStream&& asioStream) ->
        capy::any_stream
{
    struct impl : capy::any_stream::impl
    {
        typename std::decay<AsyncStream>::type stream_;

        explicit impl(AsyncStream&& s)
            : stream_(std::forward<AsyncStream>(s))
        {
        }

        capy::async_io_result
        read_some(
            capy::mutable_buffer b) override
        {
            return capy::make_async_result<capy::io_result>(
                stream_.async_read_some(b, asio::deferred));
        }

        capy::async_io_result
        write_some(
            capy::const_buffer b) override
        {
            return capy::make_async_result<capy::io_result>(
                stream_.async_write_some(b, asio::deferred));
        }
    };

    return capy::any_stream(std::make_shared<impl>(
        std::forward<AsyncStream>(asioStream)));
}

struct stream_test
{
    capy::task<int>
    t1()
    {
        co_return 67;
    }

    capy::task<int>
    do_read(capy::any_stream stream)
    {
        char buf[256];
        capy::mutable_buffer b(buf, sizeof(buf));
        auto rv = co_await stream.read_some(b);
        core::string_view sv(buf, rv.bytes_transferred);
        BOOST_TEST(! rv.ec.failed());
        BOOST_TEST_EQ(sv, "lorem ipsum");
        co_return 67;
    }

    void
    run()
    {
        asio::io_context ioc;

        spawn(
            ioc.get_executor(),
            do_read(make_stream(test::stream(ioc, "lorem ipsum"))),
            [](std::variant<std::exception_ptr, int> result)
            {
                if (result.index() == 0)
                    std::rethrow_exception(std::get<0>(result));
                BOOST_TEST_EQ(std::get<1>(result), 67);
            });

        ioc.run();
    }
};

TEST_SUITE(stream_test, "boost.beast2.stream");

} // beast2
} // boost

#endif
