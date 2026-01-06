//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_REQUEST_HPP
#define BURL_REQUEST_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast2/read.hpp>
#include <boost/beast2/write.hpp>
#include <boost/http/response_parser.hpp>
#include <boost/http/serializer.hpp>

#include <chrono>
#include <memory>

namespace asio       = boost::asio;
namespace ch         = std::chrono;
namespace beast2     = boost::beast2;
namespace http = boost::http;
using error_code     = boost::system::error_code;

template<class AsyncReadStream>
class async_request_op
{
    AsyncReadStream& stream_;
    http::serializer& serializer_;
    http::response_parser& parser_;
    ch::steady_clock::duration exp100_timeout_;
    asio::coroutine c;

    struct exp100
    {
        enum
        {
            awaiting,
            received,
            cancelled
        } state;
        asio::steady_timer timer;
    };
    std::unique_ptr<exp100> exp100_;

public:
    async_request_op(
        AsyncReadStream& stream,
        http::serializer& serializer,
        http::response_parser& parser,
        ch::steady_clock::duration exp100_timeout)
        : stream_{ stream }
        , serializer_{ serializer }
        , parser_{ parser }
        , exp100_timeout_{ exp100_timeout }
    {
    }

    template<class Self>
    void
    operator()(Self&& self, error_code ec = {}, std::size_t = {})
    {
        using asio::deferred;

        BOOST_ASIO_CORO_REENTER(c)
        {
            self.reset_cancellation_state(
                asio::enable_total_cancellation{});

            BOOST_ASIO_CORO_YIELD
            beast2::async_write(stream_, serializer_, std::move(self));

            if(!ec)
            {
                BOOST_ASIO_CORO_YIELD
                beast2::async_read_header(stream_, parser_, std::move(self));
                return self.complete(ec);
            }

            if(ec != http::error::expect_100_continue)
                return self.complete(ec);

            // TODO: use associated allocator
            exp100_.reset(new exp100{
                exp100::awaiting,
                asio::steady_timer{ stream_.get_executor() } });
            exp100_->timer.expires_after(exp100_timeout_);

            BOOST_ASIO_CORO_YIELD
            asio::experimental::make_parallel_group(
                exp100_->timer.async_wait() |
                    deferred(
                        [&stream     = stream_,
                         &serializer = serializer_,
                         exp100      = exp100_.get()](error_code)
                        {
                            return deferred
                                .when(exp100->state != exp100::cancelled)
                                .then(beast2::async_write(stream, serializer))
                                .otherwise(deferred.values(
                                    error_code{}, std::size_t{ 0 }));
                        }),
                beast2::async_read_header(stream_, parser_) |
                    deferred(
                        [&stream = stream_,
                         &parser = parser_,
                         exp100  = exp100_.get()](error_code ec, std::size_t n)
                        {
                            exp100->timer.cancel();
                            if(ec ||
                               parser.get().status() !=
                                   http::status::continue_)
                            {
                                exp100->state = exp100::cancelled;
                            }
                            else
                            {
                                exp100->state = exp100::received;
                                parser.start();
                            }
                            return deferred
                                .when(exp100->state == exp100::received)
                                .then(
                                    beast2::async_read_header(stream, parser))
                                .otherwise(deferred.values(ec, n));
                        }))
                .async_wait(
                    asio::experimental::wait_for_all(), std::move(self));
        }
    }

    template<class Self>
    void
    operator()(
        Self&& self,
        std::array<std::size_t, 2>,
        error_code ec1,
        std::size_t,
        error_code ec2,
        std::size_t)
    {
        self.complete(ec1 ? ec1 : ec2);
    }
};

template<
    class AsyncReadStream,
    typename CompletionToken = asio::default_completion_token_t<
        typename AsyncReadStream::executor_type>>
auto
async_request(
    AsyncReadStream& stream,
    http::serializer& serializer,
    http::response_parser& parser,
    ch::steady_clock::duration expect100_timeout,
    CompletionToken&& token = CompletionToken{})
{
    return asio::async_compose<CompletionToken, void(error_code)>(
        async_request_op{ stream, serializer, parser, expect100_timeout },
        token,
        stream);
}

#endif
