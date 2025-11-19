//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/read.hpp>

#include <boost/beast2/test/stream.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/bind_immediate_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/buffers/copy.hpp>
#include <boost/buffers/make_buffer.hpp>
#include <boost/rts/polystore.hpp>

#include "test_helpers.hpp"

namespace boost {
namespace beast2 {

class read_test
{
    core::string_view const msg =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "abc";
public:
    void
    testAsyncReadSome()
    {
        boost::asio::io_context ioc;
        boost::rts::polystore rts_ctx;
        http_proto::install_parser_service(rts_ctx, {});

        // async_read_some completes when the parser reads
        // the header section of the message.
        {
            test::stream ts(ioc, msg);
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // limit async_read_some for better coverage
            ts.read_size(1);

            // header
            async_read_some(
                ts,
                pr,
                [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST(! ec.failed());
                    BOOST_TEST_EQ(n, msg.size() - 3); // minus body
                });
            test::run(ioc);
            BOOST_TEST(pr.got_header());
            BOOST_TEST(! pr.is_complete());

            // body
            for(auto i = 0; i < 3; i++)
            {
                async_read_some(
                    ts,
                    pr,
                    [&](system::error_code ec, std::size_t n)
                    {
                        BOOST_TEST(! ec.failed());
                        BOOST_TEST_EQ(n, 1); // because of ts.read_size(1)
                    });
                BOOST_TEST_EQ(test::run(ioc), 1);
            }
            BOOST_TEST(pr.is_complete());
            BOOST_TEST(pr.body() == "abc");
        }

        // async_read_some reports stream errors
        {
            test::fail_count fc(11, asio::error::network_down);
            test::stream ts(ioc, fc, msg);
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // limit async_read_some for better coverage
            ts.read_size(1);

            bool invoked = false;
            async_read_some(
                ts,
                pr,
                [&](system::error_code ec, std::size_t n)
                {
                    invoked = true;
                    BOOST_TEST_EQ(ec, asio::error::network_down);
                    BOOST_TEST_EQ(n, 10);
                });
            BOOST_TEST_EQ(test::run(ioc), 11);
            BOOST_TEST(invoked);
        }

        // async_read_some reports parser errors
        {
            test::stream ts(ioc, msg);
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // read header
            async_read_some(ts, pr, test::success_handler());
            test::run(ioc);

            // read body
            pr.set_body_limit(2);
            async_read_some(
                ts,
                pr,
                test::fail_handler(http_proto::error::body_too_large));
            test::run(ioc);
        }

        // async_read_some cancellation
        {
            test::stream ts(ioc);
            asio::cancellation_signal c_signal;
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // async_read_some cancels after reading 0 bytes
            async_read_some(
                ts,
                pr,
                asio::bind_cancellation_slot(
                    c_signal.slot(),
                    [](system::error_code ec, std::size_t n)
                    {
                        BOOST_TEST_EQ(n, 0);
                        BOOST_TEST_EQ(ec, asio::error::operation_aborted);
                    }));
            c_signal.emit(asio::cancellation_type::total);
            test::run(ioc);

            // append 8 bytes of the msg
            ts.append(msg.substr(0, 8));

            // async_read_some cancels after reading 8 bytes
            async_read_some(
                ts,
                pr,
                asio::bind_cancellation_slot(
                    c_signal.slot(),
                    [](system::error_code ec, std::size_t n)
                    {
                        BOOST_TEST_EQ(n, 8);
                        BOOST_TEST_EQ(ec, asio::error::operation_aborted);
                    }));
            c_signal.emit(asio::cancellation_type::total);
            test::run(ioc);

            // append rest of the msg
            ts.append(msg.substr(8, msg.npos));

            // async_read_some succeeds
            async_read_some(ts, pr, test::success_handler());
            test::run(ioc);
            BOOST_TEST(pr.got_header());
        }
    }

    void
    testAsyncReadHeader()
    {
        // currently, async_read_header and
        // async_read_some are identical
    }

    void
    testAsyncRead()
    {
        boost::asio::io_context ioc;
        rts::polystore rts_ctx;
        http_proto::install_parser_service(rts_ctx, {});

        // async_read completes when the parser reads
        // the entire message.
        {
            test::stream ts(ioc, msg);
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // limit async_read_some for better coverage
            ts.read_size(1);

            async_read(
                ts,
                pr,
                [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST(! ec.failed());
                    BOOST_TEST_EQ(n, msg.size());
                });

            test::run(ioc);

            BOOST_TEST_EQ(ts.nread(), msg.size()); // because of ts.read_size(1)
            BOOST_TEST(pr.is_complete());
            BOOST_TEST(pr.body() == "abc");
        }

        // async_read completes immediatly when
        // parser contains enough data
        {
            asio::post(
                ioc,
                [&]()
                {
                    test::stream ts(ioc);
                    http_proto::response_parser pr(rts_ctx);
                    pr.reset();
                    pr.start();

                    pr.commit(
                        buffers::copy(
                            pr.prepare(),
                            buffers::const_buffer(
                                msg.data(),
                                msg.size())));

                    async_read(
                        ts,
                        pr,
                        asio::bind_immediate_executor(
                            ioc.get_executor(),
                            test::success_handler()));

                    BOOST_TEST_EQ(ts.nread(), 0);
                    BOOST_TEST(pr.is_complete());
                    BOOST_TEST(pr.body() == "abc");
                });
            BOOST_TEST_EQ(test::run(ioc), 1);
        }
    }

    void
    run()
    {
        testAsyncReadSome();
        testAsyncReadHeader();
        testAsyncRead();
    }
};

TEST_SUITE(read_test, "boost.beast2.read");

} // beast2
} // boost
