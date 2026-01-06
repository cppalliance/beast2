//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/write.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

//class any_async_read_stream
//{
//};
//
//class write_test
//{
//public:
//    void
//    testWrite()
//    {
//    }
//
//    void
//    run()
//    {
//        testWrite();
//    }
//};
//
//TEST_SUITE(
//    write_test,
//    "boost.beast2.write");

} // beast2
} // boost

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
#include <boost/beast2/write.hpp>

#include <boost/array.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/bind_immediate_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast2/test/stream.hpp>
#include <boost/buffers/copy.hpp>
#include <boost/buffers/make_buffer.hpp>
#include <boost/buffers/string_buffer.hpp>
#include <boost/capy/polystore.hpp>
#include <boost/http/response.hpp>

#include "test_helpers.hpp"

#include <iostream>

namespace boost {
namespace beast2 {

class write_test
{
    core::string_view const headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 3\r\n"
        "\r\n";
    core::string_view const body =
        "abc";
    core::string_view const msg =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "abc";

public:
    void
    testAsyncWriteSome()
    {
        boost::asio::io_context ioc;
        boost::capy::polystore capy_ctx;
        http::install_serializer_service(capy_ctx, {});

        // async_write_some completes when the serializer writes the message.
        {
            test::stream ts{ ioc }, tr{ ioc };
            ts.connect(tr);
            ts.write_size(1);

            http::serializer sr(capy_ctx);
            sr.reset();

            http::response res(headers);
            sr.start(res, buffers::const_buffer(body.data(), body.size()));

            for(std::size_t total = 0; total < msg.size(); total++)
            {
                async_write_some(
                    ts,
                    sr,
                    [&](system::error_code ec, std::size_t n)
                    {
                        BOOST_TEST(!ec.failed());
                        BOOST_TEST_EQ(n, 1);
                    });
                test::run(ioc);
            }
            BOOST_TEST(sr.is_done());
            BOOST_TEST_EQ(tr.str(), msg);

            BOOST_TEST_EQ(tr.str(), msg);
        }

        // async_write_some reports stream errors
        {
            test::fail_count fc(3, asio::error::network_down);
            test::stream ts{ ioc, fc }, tr{ ioc };
            ts.connect(tr);
            ts.write_size(1);

            http::serializer sr(capy_ctx);
            sr.reset();

            http::response res(headers);
            sr.start(res, buffers::const_buffer(body.data(), body.size()));

            for(int count = 0; count < 3; count++)
            {
                async_write_some(
                    ts,
                    sr,
                    [&](system::error_code ec, std::size_t n)
                    {
                        if (count < 2)
                        {
                            BOOST_TEST(!ec.failed());
                            BOOST_TEST_EQ(n, 1);
                        }
                        else
                        {
                            BOOST_TEST_EQ(ec, asio::error::network_down);
                            BOOST_TEST_EQ(n, 0);
                        }
                    });
                test::run(ioc);

                auto expected = msg.substr(0, (count == 0) ? 1 : 2);
                BOOST_TEST_EQ(tr.str(), expected);
            }
        }

        // async_write_some cancellation
        {
            boost::array<asio::cancellation_type, 3> ctypes{
                { asio::cancellation_type::total,
                  asio::cancellation_type::partial,
                  asio::cancellation_type::terminal }};

            for(auto ctype : ctypes)
            {
                test::stream ts{ ioc }, tr{ ioc };
                ts.connect(tr);
                ts.write_size(5);

                asio::cancellation_signal c_signal;

                http::serializer sr(capy_ctx);
                sr.reset();

                http::response res(headers);
                sr.start(res, buffers::const_buffer(body.data(), body.size()));

                // async_read_some cancels after reading 0 bytes
                async_write_some(
                    ts,
                    sr,
                    asio::bind_cancellation_slot(
                        c_signal.slot(),
                        [](system::error_code ec, std::size_t n)
                        {
                            BOOST_TEST_EQ(n, 5);
                            BOOST_TEST(!ec.failed());
                        }));
                c_signal.emit(ctype);

                test::run(ioc);

                BOOST_TEST_EQ(tr.str(), "HTTP/");
            }
        }
    }

    void
    testAsyncWrite()
    {
        boost::asio::io_context ioc;
        capy::polystore capy_ctx;
        http::install_serializer_service(capy_ctx, {});

        // async_write completes when the serializer writes
        // the entire message.
        {
            test::stream ts{ ioc }, tr{ ioc };
            ts.connect(tr);
            ts.write_size(1);

            http::serializer sr(capy_ctx);
            sr.reset();

            http::response res(headers);
            sr.start(res, buffers::const_buffer(body.data(), body.size()));

            async_write(
                ts,
                sr,
                [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST(!ec.failed());
                    BOOST_TEST_EQ(n, msg.size());
                });

            test::run(ioc);

            BOOST_TEST_EQ(ts.nwrite(), msg.size()); // because of ts.write_size(1)
            BOOST_TEST(sr.is_done());
            BOOST_TEST_EQ(tr.str(), msg);
        }

        // async_write reports stream errors
        {
            test::fail_count fc(3, asio::error::network_down);
            test::stream ts{ ioc, fc }, tr{ ioc };
            ts.connect(tr);
            ts.write_size(1);

            http::serializer sr(capy_ctx);
            sr.reset();

            http::response res(headers);
            sr.start(res, buffers::const_buffer(body.data(), body.size()));

            async_write(
                ts,
                sr,
                [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST_EQ(ec, asio::error::network_down);
                    BOOST_TEST_EQ(n, 2);
                });
            test::run(ioc);

            auto expected = msg.substr(0, 2);
            BOOST_TEST_EQ(tr.str(), expected);

        }

        // async_write cancellation
        {
            boost::array<asio::cancellation_type, 3> ctypes{
                { asio::cancellation_type::total,
                  asio::cancellation_type::partial,
                  asio::cancellation_type::terminal }};

            for(auto ctype : ctypes)
            {
                test::stream ts{ ioc }, tr{ ioc };
                ts.connect(tr);
                ts.write_size(5);

                asio::cancellation_signal c_signal;

                http::serializer sr(capy_ctx);
                sr.reset();

                http::response res(headers);
                sr.start(res, buffers::const_buffer(body.data(), body.size()));

                // cancel after writing
                async_write(
                    ts,
                    sr,
                    asio::bind_cancellation_slot(
                        c_signal.slot(),
                        [](system::error_code ec, std::size_t n)
                        {
                            BOOST_TEST_EQ(n, 5);
                            BOOST_TEST_EQ(ec, asio::error::operation_aborted);
                        }));
                c_signal.emit(ctype);

                test::run(ioc);

                BOOST_TEST_EQ(tr.str(), "HTTP/");
            }
        }
    }

    void
    run()
    {
        testAsyncWriteSome();
        testAsyncWrite();
    }
};

TEST_SUITE(write_test, "boost.beast2.write");

} // beast2
} // boost
