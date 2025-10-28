//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//


#include <boost/beast2/body_read_stream.hpp>

//#include <boost/beast/core/flat_buffer.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/buffers/buffer.hpp>
#include <boost/buffers/circular_buffer.hpp>
#include <boost/buffers/copy.hpp>
#include <boost/buffers/make_buffer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/static_assert.hpp>
#include <boost/rts/context.hpp>
#include <boost/utility/string_view.hpp>

//#include <boost/asio/basic_socket.hpp>
//#include <boost/asio/ip/tcp.hpp>

#include <boost/beast2/test/stream.hpp>
#include "test_helpers.hpp"

#include <algorithm>
#include <iostream>

namespace boost {

#if 0

template<class Executor>
struct MockReadStream {
    MockReadStream(Executor& ex, const std::string &data, std::size_t chunk) : ex_(ex), mock_data_(data), chunk_(chunk), sent_(0)
    {
    }

    typedef Executor executor_type;
    
    Executor get_executor() { return ex_; }

    //template <typename Token> auto async_write_some(asio::const_buffer buf, Token&& token) {
    //    return asio::async_initiate<Token, void(system::error_code, size_t)>( //
    //        [&ex_](auto h, auto buf) {
    //            asio::dispatch(ex_, [=, h = std::move(h)]() mutable {
    //                std::move(h)({}, asio::buffer_size(buf));
    //                });
    //        },
    //        token, buf);
    //}

    template<
        class MutableBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(
            void(system::error_code, std::size_t)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
        void(system::error_code, std::size_t))
    async_read_some(
          const MutableBufferSequence& buf
        , CompletionToken&& token)
    {
        return asio::async_initiate<CompletionToken, void(system::error_code, size_t)>(
            [this](
                CompletionToken&& token
                ,  const MutableBufferSequence& buf)
            {
                boost::string_view source_str{
                    mock_data_.data() + sent_,
                    mock_data_.size() - sent_ };
                auto source_buf = asio::buffer(source_str);

                std::size_t n = asio::buffer_copy(buf, source_buf, chunk_);

                system::error_code ec = (n != 0) ? system::error_code{} : asio::stream_errc::eof;

                sent_ += n;

                asio::post(ex_, asio::prepend(std::move(token), ec, n));
            },
            token, buf);
    }

    Executor& ex_;
    std::string mock_data_;
    std::size_t sent_;
    std::size_t chunk_;
};

namespace beast2 {

struct body_read_stream_test_simple
{
    void
    run()
    {
        std::string header = "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Last-Modified: Thu, 09 Oct 2025 16:42:02 GMT\r\n"
            "Cache-Control: max-age=86000\r\n"
            "Date: Thu, 16 Oct 2025 15:09:10 GMT\r\n"
            "Content-Length: 60\r\n"
            "Connection: keep-alive\r\n"
            "\r\n";

        std::string body = 
            "<!doctype html><html><head><title>Hello World</title></html>\r\n";

        std::string data = header + body;

        std::cout << "header size: " << header.size() << std::endl;
        std::cout << "body size: " << body.size() << std::endl;
        std::cout << "data size: " << data.size() << std::endl;

        for (std::size_t chunk = 3; chunk < 400; chunk++)
        {
            asio::io_context ioc;
            auto strand = asio::make_strand(ioc);
            MockReadStream<decltype(strand)> ms(strand, data, chunk);

            std::array<char, 41> arr;
            auto buf = asio::buffer(arr);

            rts::context rts_ctx;
            http_proto::response_parser::config cfg;
            cfg.body_limit = 1024 * 1024;
            cfg.min_buffer = 1024 * 1024;
            http_proto::install_parser_service(rts_ctx, cfg);

            http_proto::response_parser pr(rts_ctx);
            pr.reset();
            pr.start();

            body_read_stream<decltype(ms)> brs(ms, pr);

            brs.async_read_some(buf,
                [this, &chunk, &brs, &arr, &buf](system::error_code ec, std::size_t bytes_transferred)
                {
                    if (ec.failed()) std::cerr << ec.message() << std::endl;

                    BOOST_TEST_EQ(ec.failed(), false);
                    BOOST_TEST_GE(bytes_transferred, 1);
                    {
                        std::string value(arr.data(), bytes_transferred);
                        std::cout << chunk << ": " << value << std::endl;
                        //BOOST_TEST_EQ(value, std::string("<!doctype html><html><head><title>Hello W"));
                    }

                    if (!ec.failed()) {
                        brs.async_read_some(buf,
                            [this, &brs, &arr, &buf](system::error_code ec, std::size_t bytes_transferred)
                            {
                                if (ec.failed()) std::cerr << ec.message() << std::endl;

                                BOOST_TEST_EQ(ec.failed(), false);
                                BOOST_TEST_GE(bytes_transferred, 1);
                                {
                                    std::string value(arr.data(), bytes_transferred);
                                    //BOOST_TEST_EQ(value, std::string("orld</title></html>"));
                                }
                            });
                    }
                });
            ioc.run();
        }
    }
};

} // namespace beast2

#endif

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
            for (std::size_t cs = 240; cs < msg_length_ + 2; cs++)
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
            //async_read_some(
            //    ts,
            //    pr,
            //    test::fail_handler(http_proto::error::body_too_large));
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

} // http_io
} // boost
