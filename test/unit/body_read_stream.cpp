//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//


#include <boost/beast2/body_read_stream.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/buffers/buffer.hpp>
#include <boost/buffers/circular_buffer.hpp>
#include <boost/buffers/copy.hpp>
#include <boost/buffers/make_buffer.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/post.hpp>
#include <boost/static_assert.hpp>
#include <boost/rts/context.hpp>
#include <boost/utility/string_view.hpp>

#include <boost/beast2/test/stream.hpp>
#include "test_helpers.hpp"

#include <algorithm>
#include <iostream>

namespace boost {
namespace beast2 {

template<class Buffers>
std::string
    test_to_string(Buffers const& bs)
{
    std::string s(
        buffers::size(bs), 0);
    s.resize(buffers::copy(
        buffers::make_buffer(&s[0], s.size()),
        bs));
    return s;
}


class body_read_stream_test
{
private:
    std::string header_ = "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Last-Modified: Thu, 09 Oct 2025 16:42:02 GMT\r\n"
        "Cache-Control: max-age=86000\r\n"
        "Date: Thu, 16 Oct 2025 15:09:10 GMT\r\n"
        "Content-Length: 60\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    std::string body_ =
        "<!doctype html><html><head><title>Hello World</title></html>";

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
            for (std::size_t cs = 1; cs < msg_length_ + 2; cs++)
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
                for (std::size_t i = 0; i < body_length_; i++)
                {
                    brs.async_read_some(
                        buf.prepare(1024),
                        [&](system::error_code ec, std::size_t n)
                        {
                            if (total < body_length_) {
                                if (ec.failed()) std::cerr << ec.message() << std::endl;
                                BOOST_TEST(!ec.failed());
                                std::size_t expected = cs;
                                // In the first iteration we remove any of the data that was
                                // associcated with the headers.
                                if (i == 0) {
                                    expected = expected - (header_length_ % cs);
                                    // The `beast2::async_read_some` will always read move from the wire
                                    // immediately after the headers, even if we have a partial body
                                    // in memory already.
                                    // This should be removable once `async_read_some` changes.
                                    if (expected < cs) expected += cs;
                                }
                                expected = std::min(expected, body_length_ - total);
                                BOOST_TEST_EQ(n, expected); // because of ts.read_size(x)
                                if (n != expected) std::cerr << "cs: " << cs << " i: " << i << "\n" << std::endl;
                                buf.commit(n);
                                total += n;
                            }
                        });
                    auto count = test::run(ioc);
                    if (i > 0)
                        BOOST_TEST_EQ(count, 1);
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
            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            std::string m1 = header_ + body_.substr(0, 10);
            test::stream ts(ioc, m1);

            // The object under test
            body_read_stream<test::stream> brs(ts, pr);

            // Create a destination buffer
            std::string s;
            s.reserve(2048);
            boost::buffers::string_buffer buf(&s);

            int invoked2 = 0;

            auto lambda2 = [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST_EQ(n, 0);
                    BOOST_TEST_EQ(ec, http_proto::error::incomplete);
                    invoked2++;
                };

            int invoked1 = 0;

            auto lambda1 = [&](system::error_code ec, std::size_t n)
                {
                    BOOST_TEST_EQ(n, 0);
                    BOOST_TEST_EQ(ec, asio::error::operation_aborted);
                    invoked1++;
                    brs.async_read_some(
                        buf.prepare(1024),
                        lambda2);
                };

            brs.async_read_some(
                buf.prepare(1024),
                asio::cancel_after(std::chrono::milliseconds{ 250 }, lambda1));

            test::run_for(ioc, std::chrono::seconds{ 5 });

            BOOST_TEST_EQ(invoked1, 1);
            BOOST_TEST_EQ(invoked2, 1);
            BOOST_TEST(pr.got_header());
            BOOST_TEST(!pr.is_complete());

            std::cout << "done" << std::endl;
        }

        // Test asio::async_read works
        {
            // pick a representative chunk, as we already do the looping over all chunks above.
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

            asio::async_read(brs,
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
        //testAsyncReadHeader();
        //testAsyncRead();
    }
};


TEST_SUITE(
    body_read_stream_test,
    "boost.http_io.body_read_stream.test");

} // beast2
} // boost
