//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//

#include <boost/http_io/body_read_stream.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/buffers/buffer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/static_assert.hpp>
#include <boost/rts/context.hpp>
#include <boost/utility/string_view.hpp>

#include "test_suite.hpp"

#include <algorithm>
#include <iostream>

namespace boost {

template<class Executor>
struct MockReadStream {
    MockReadStream(Executor& ex, const std::string &data) : ex_(ex), mock_data_(data), sent_(0)
    {
    }
    
    Executor get_executor() const { return ex_; }

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

                std::size_t chunk_size = std::max(
                    (std::size_t)(rand() % mock_data_.size()),
                    (std::size_t)1);

                std::size_t n = asio::buffer_copy(buf, source_buf, chunk_size);

                system::error_code ec = (n != 0) ? system::error_code{} : asio::stream_errc::eof;

                sent_ += n;

                asio::post(ex_, asio::prepend(std::move(token), ec, n));
            },
            token, buf);
    }

    Executor& ex_;
    std::string mock_data_;
    std::size_t sent_;
};

namespace http_io {

struct body_read_stream_test
{
    void
    run()
    {
        std::string data = "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Last-Modified: Thu, 09 Oct 2025 16:42:02 GMT\r\n"
            "Cache-Control: max-age=86000\r\n"
            "Date: Thu, 16 Oct 2025 15:09:10 GMT\r\n"
            "Content-Length: 60\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "<!doctype html><html><head><title>Hello World</title></html>\r\n";

        asio::io_context ioc;
        auto strand = asio::make_strand(ioc);
        MockReadStream<decltype(strand)> ms(strand, data);

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
            [this, &brs, &arr, &buf](system::error_code ec, std::size_t bytes_transferred)
            {
                if (ec.failed()) std::cerr << ec.message() << std::endl;

                BOOST_TEST_EQ(ec.failed(), false);
                BOOST_TEST_EQ(bytes_transferred, 41);
                {
                    std::string value(arr.data(), bytes_transferred);
                    BOOST_TEST_EQ(value, std::string("<!doctype html><html><head><title>Hello W"));
                }

                brs.async_read_some(buf,
                    [this, &brs, &arr, &buf](system::error_code ec, std::size_t bytes_transferred)
                    {
                        if (ec.failed()) std::cerr << ec.message() << std::endl;

                        BOOST_TEST_EQ(ec.failed(), false);
                        BOOST_TEST_EQ(bytes_transferred, 19);
                        {
                            std::string value(arr.data(), bytes_transferred);
                            BOOST_TEST_EQ(value, std::string("orld</title></html>"));
                        }
                    });
            });
        ioc.run();
    }
};

TEST_SUITE(
    body_read_stream_test,
    "boost.http_io.body_read_stream.hello_world");

} // http_io
} // boost
