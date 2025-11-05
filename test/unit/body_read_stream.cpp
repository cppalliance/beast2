//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/body_read_stream.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/read.hpp>
#include <boost/buffers/make_buffer.hpp>
#include <boost/rts/context.hpp>

#include "test_helpers.hpp"
#include <boost/beast2/test/stream.hpp>

#include <algorithm>
#include <iostream>

namespace boost
{
namespace beast2
{

template<class Buffers>
std::string
test_to_string(Buffers const& bs)
{
    std::string s(buffers::size(bs), 0);
    s.resize(buffers::copy(buffers::make_buffer(&s[0], s.size()), bs));
    return s;
}

class body_read_stream_test
{
private:
    std::string body_ = "Hello World!";

    std::string header_ = "HTTP/1.1 200 OK\r\n"
                          "Content-Length: 12\r\n"
                          "\r\n";

    std::string msg_ = header_ + body_;

    std::size_t header_length_ = header_.size();
    std::size_t body_length_ = body_.size();
    std::size_t msg_length_ = msg_.size();

public:
    void
    testAsyncReadSome()
    {
        boost::asio::io_context ioc;
        rts::context rts_ctx;
        http_proto::install_parser_service(rts_ctx, {});

        // async_read_some reads the body for various chunk
        // sizes.
        {
            for(std::size_t cs = 1; cs < msg_length_ + 2; cs++)
            {
                test::stream ts(ioc, msg_);
                http_proto::response_parser pr(rts_ctx);
                pr.reset();
                pr.start();

                // limit async_read_some for better coverage
                ts.read_size(cs);

                // Create a destination buffer
                std::string s;
                s.reserve(2048);
                boost::buffers::string_buffer buf(&s);

                // The object under test
                body_read_stream<test::stream> brs(ts, pr);

                // run the test
                std::size_t total = 0;
                for(std::size_t i = 0; i < body_length_; i++)
                {
                    brs.async_read_some(
                        buf.prepare(1024),
                        [&](system::error_code ec, std::size_t n)
                        {
                            if(total < body_length_)
                            {
                                if(ec.failed())
                                {
                                    std::cerr << ec.message() << std::endl;
                                }
                                BOOST_TEST(!ec.failed());
                                std::size_t expected = cs;
                                // In the first iteration we remove any of the
                                // data that was associcated with the headers.
                                if(i == 0)
                                {
                                    expected = expected - (header_length_ % cs);
                                    // The `beast2::async_read_some` will always
                                    // read move from the wire immediately after
                                    // the headers, even if we have a partial
                                    // body in memory already. This should be
                                    // removable once `async_read_some` changes.
                                    if(expected < cs)
                                    {
                                        expected += cs;
                                    }
                                }
                                expected =
                                    std::min(expected, body_length_ - total);
                                BOOST_TEST_EQ(
                                    n, expected); // because of ts.read_size(x)
                                if(n != expected)
                                {
                                    std::cerr << "cs: " << cs << " i: " << i
                                              << "\n"
                                              << std::endl;
                                }
                                buf.commit(n);
                                total += n;
                            }
                        });
                    auto count = test::run(ioc);
                    if(i > 0)
                    {
                        BOOST_TEST_EQ(count, 1);
                    }
                    BOOST_TEST(pr.got_header());
                }

                BOOST_TEST(pr.is_complete());
                BOOST_TEST_EQ(buf.size(), body_length_);
                BOOST_TEST_EQ(total, body_length_);

                std::string result(test_to_string(buf.data()));

                BOOST_TEST(result == body_);
                BOOST_TEST_EQ(result.size(), body_length_);
            }
        }

        // Test async_read_some cancellation
        {
            for(std::size_t i = 1; i < msg_length_; i++)
            {
                http_proto::response_parser pr(rts_ctx);
                pr.reset();
                pr.start();

                std::string m1 = msg_.substr(0, i);
                test::stream ts(ioc, m1);

                // The object under test
                body_read_stream<test::stream> brs(ts, pr);

                // Create a destination buffer
                std::string s;
                s.reserve(2048);
                boost::buffers::string_buffer buf(&s);

                int invoked1 = 0;

                auto lambda1 = [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST_EQ(n, 0);
                    BOOST_TEST_EQ(ec, asio::error::operation_aborted);

                    invoked1++;
                };

                // Add a signal to test cancellation
                asio::cancellation_signal c_signal;

                brs.async_read_some(
                    buf.prepare(1024),
                    asio::bind_cancellation_slot(c_signal.slot(), lambda1));

                // send a cancellation
                c_signal.emit(asio::cancellation_type::total);

                // Run up until the point of cancellation.
                test::run(ioc);

                BOOST_TEST_EQ(invoked1, 1);
                BOOST_TEST(pr.got_header() == (i >= header_length_));
                BOOST_TEST(!pr.is_complete());

                int invoked2 = 0;

                auto lambda2 = [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST_EQ(n, body_length_);
                    BOOST_TEST(!ec.failed());

                    invoked2++;
                };

                // Append the remainder of the message and try again.
                std::string remainder = msg_.substr(i);
                ts.append(remainder);
                brs.async_read_some(buf.prepare(1024), lambda2);

                // Continue running until the end.
                test::run(ioc);

                BOOST_TEST_EQ(invoked2, 1);
                BOOST_TEST(pr.got_header());
                BOOST_TEST(pr.is_complete());
            }
        }

        // Test asio::async_read works
        {
            // pick a representative chunk, as we already do the looping over
            // all chunks above.
            std::size_t cs = 5;

            test::stream ts(ioc, msg_);
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // limit async_read_some for better coverage
            ts.read_size(cs);

            // The object under test
            body_read_stream<test::stream> brs(ts, pr);

            // Create a destination buffer
            std::string s;
            s.reserve(2048);
            boost::buffers::string_buffer buf(&s);

            int invoked = 0;

            asio::async_read(
                brs,
                asio::buffer(buf.prepare(1024).data(), 1024),
                [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST(ec == asio::error::eof);
                    BOOST_TEST(n == body_length_);
                    buf.commit(n);
                    invoked++;
                });

            test::run(ioc);

            BOOST_TEST_EQ(invoked, 1);

            BOOST_TEST(pr.got_header());
            BOOST_TEST(pr.is_complete());
            BOOST_TEST_EQ(buf.size(), body_length_);

            std::string result(test_to_string(buf.data()));

            BOOST_TEST(result == body_);
            BOOST_TEST_EQ(result.size(), body_length_);
        }

        // async_read_some reports stream errors
        {
            test::fail_count fc(11, asio::error::network_down);
            test::stream ts(ioc, fc, msg_);
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // limit async_read_some for better coverage
            ts.read_size(1);

            // The object under test
            body_read_stream<test::stream> brs(ts, pr);

            // Create a destination buffer
            std::string s;
            s.reserve(2048);
            boost::buffers::string_buffer buf(&s);

            bool invoked = false;
            brs.async_read_some(
                buf.prepare(1024),
                [&](system::error_code ec, std::size_t n)
                {
                    invoked = true;
                    BOOST_TEST_EQ(ec, asio::error::network_down);
                    BOOST_TEST_EQ(n, 0);
                });
            BOOST_TEST_EQ(test::run(ioc), 11);
            BOOST_TEST(invoked);
        }

        // async_read_some reports parser errors
        {
            test::stream ts(ioc, msg_);
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            // Ensure we get an error
            pr.set_body_limit(2);

            // The object under test
            body_read_stream<test::stream> brs(ts, pr);

            // Create a destination buffer
            std::string s;
            s.reserve(2048);
            boost::buffers::string_buffer buf(&s);

            // read body
            pr.set_body_limit(2);
            brs.async_read_some(
                buf.prepare(1024),
                test::fail_handler(http_proto::error::body_too_large));
            test::run(ioc);
        }
    }

    void
    run()
    {
        testAsyncReadSome();
    }
};

TEST_SUITE(body_read_stream_test, "boost.beast2.body_read_stream");

} // beast2
} // boost
