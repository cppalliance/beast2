//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_ANY_STREAM_HPP
#define BURL_ANY_STREAM_HPP

#include <boost/asio/append.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <variant>

namespace asio   = boost::asio;
namespace ssl    = boost::asio::ssl;
using error_code = boost::system::error_code;

class any_stream
{
public:
    using executor_type     = asio::any_io_executor;
    using plain_stream_type = asio::ip::tcp::socket;
    using ssl_stream_type   = ssl::stream<plain_stream_type>;

    any_stream(plain_stream_type stream)
        : stream_{ std::move(stream) }
    {
    }

    any_stream(ssl_stream_type stream)
        : stream_{ std::move(stream) }
    {
    }

    executor_type
    get_executor() noexcept
    {
        return std::visit([](auto& s) { return s.get_executor(); }, stream_);
    }

    template<
        typename ConstBufferSequence,
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_write_some(
        const ConstBufferSequence& buffers,
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::
            async_compose<CompletionToken, void(error_code, size_t)>(
                [this, buffers, init = false](
                    auto&& self, error_code ec = {}, size_t n = 0) mutable
                {
                    if(std::exchange(init, true))
                        return self.complete(ec, n);

                    std::visit(
                        [&](auto& s)
                        { s.async_write_some(buffers, std::move(self)); },
                        stream_);
                },
                token,
                get_executor());
    }

    template<
        typename MutableBufferSequence,
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_read_some(
        const MutableBufferSequence& buffers,
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::
            async_compose<CompletionToken, void(error_code, size_t)>(
                [this, buffers, init = false](
                    auto&& self, error_code ec = {}, size_t n = 0) mutable
                {
                    if(std::exchange(init, true))
                        return self.complete(ec, n);

                    std::visit(
                        [&](auto& s)
                        { s.async_read_some(buffers, std::move(self)); },
                        stream_);
                },
                token,
                get_executor());
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_shutdown(
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::async_compose<CompletionToken, void(error_code)>(
            [this, init = false](auto&& self, error_code ec = {}) mutable
            {
                if(std::exchange(init, true))
                    return self.complete(ec);

                std::visit(
                    [&](auto& s)
                    {
                        if constexpr(
                            std::is_same_v<decltype(s),ssl_stream_type&>)
                        {
                            s.async_shutdown(std::move(self));
                        }
                        else
                        {
                            s.close(ec);
                            asio::async_immediate(
                                s.get_executor(),
                                asio::append(std::move(self), ec));
                        }
                    },
                    stream_);
            },
            token,
            get_executor());
    }

private:
    std::variant<plain_stream_type, ssl_stream_type> stream_;
};

#endif
