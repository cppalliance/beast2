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

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/buffers/algorithm.hpp>
#include <boost/http_proto/parser.hpp>
#include <boost/http_proto/serializer.hpp>

namespace asio       = boost::asio;
namespace buffers    = boost::buffers;
namespace http_proto = boost::http_proto;
using error_code     = boost::system::error_code;

class any_stream
{
public:
    using const_buffers_type   = http_proto::serializer::const_buffers_type;
    using mutable_buffers_type = http_proto::parser::mutable_buffers_type;
    using executor_type        = asio::any_io_executor;

    template<typename Stream>
    any_stream(Stream stream)
        : rd_timer{ stream.get_executor() }
        , wr_timer{ stream.get_executor() }
    {
        class impl : public base
        {
            Stream stream_;

        public:
            impl(Stream stream)
                : stream_{ std::move(stream) }
            {
            }

            virtual asio::any_io_executor
            get_executor() override
            {
                return stream_.get_executor();
            }

            virtual void
            async_write_some(
                const buffers::const_buffer_subspan& buffers,
                asio::any_completion_handler<void(error_code, std::size_t)>
                    handler) override
            {
                stream_.async_write_some(buffers, std::move(handler));
            }

            virtual void
            async_read_some(
                const buffers::mutable_buffer_subspan& buffers,
                asio::any_completion_handler<void(error_code, std::size_t)>
                    handler) override
            {
                stream_.async_read_some(buffers, std::move(handler));
            }

            virtual void
            async_shutdown(
                asio::any_completion_handler<void(error_code)> handler) override
            {
                if constexpr(requires { typename Stream::handshake_type; })
                {
                    stream_.async_shutdown(std::move(handler));
                }
                else
                {
                    asio::async_compose<decltype(handler), void(error_code)>(
                        [this,
                         init = false](auto&& self, error_code ec = {}) mutable
                        {
                            if(std::exchange(init, true))
                                return self.complete(ec);

                            stream_.close(ec);
                            asio::async_immediate(
                                stream_.get_executor(),
                                asio::append(std::move(self), ec));
                        },
                        handler,
                        stream_);
                }
            }
        };

        stream_ = std::make_unique<impl>(std::move(stream));
    }

    executor_type
    get_executor() const
    {
        return stream_->get_executor();
    }

    void
    read_limit(std::size_t bytes_per_second) noexcept
    {
        rd_limit_ = bytes_per_second;
        if(rd_remain_ > bytes_per_second)
            rd_remain_ = bytes_per_second;
    }

    void
    write_limit(std::size_t bytes_per_second) noexcept
    {
        wr_limit_ = bytes_per_second;
        if(wr_remain_ > bytes_per_second)
            wr_remain_ = bytes_per_second;
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_write_some(
        const const_buffers_type& buffers,
        CompletionToken&& token = CompletionToken{})
    {
        return asio::
            async_compose<CompletionToken, void(error_code, std::size_t)>(
                [this, c = asio::coroutine{}, buffers](
                    auto&& self, error_code ec = {}, std::size_t n = {}) mutable
                {
                    BOOST_ASIO_CORO_REENTER(c)
                    {
                        if(wr_remain_ == 0)
                        {
                            wr_timer.expires_after(std::chrono::seconds{ 1 });
                            BOOST_ASIO_CORO_YIELD
                            wr_timer.async_wait(std::move(self));
                            wr_remain_ = wr_limit_;
                        }

                        BOOST_ASIO_CORO_YIELD
                        stream_->async_write_some(
                            buffers::prefix(buffers, wr_remain_),
                            std::move(self));

                        wr_remain_ -= n;
                        self.complete(ec, n);
                    }
                },
                token,
                get_executor());
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_read_some(
        const mutable_buffers_type& buffers,
        CompletionToken&& token = CompletionToken{})
    {
        return asio::
            async_compose<CompletionToken, void(error_code, std::size_t)>(
                [this, c = asio::coroutine{}, buffers](
                    auto&& self, error_code ec = {}, std::size_t n = {}) mutable
                {
                    BOOST_ASIO_CORO_REENTER(c)
                    {
                        if(rd_remain_ == 0)
                        {
                            rd_timer.expires_after(std::chrono::seconds{ 1 });
                            BOOST_ASIO_CORO_YIELD
                            rd_timer.async_wait(std::move(self));
                            rd_remain_ = rd_limit_;
                        }

                        BOOST_ASIO_CORO_YIELD
                        stream_->async_read_some(
                            buffers::prefix(buffers, rd_remain_),
                            std::move(self));

                        rd_remain_ -= n;
                        self.complete(ec, n);
                    }
                },
                token,
                get_executor());
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_shutdown(CompletionToken&& token = CompletionToken{})
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            [this, init = false](auto&& self, error_code ec = {}) mutable
            {
                if(std::exchange(init, true))
                    return self.complete(ec);
                stream_->async_shutdown(std::move(self));
            },
            token,
            get_executor());
    }

private:
    struct base
    {
        virtual asio::any_io_executor
        get_executor() = 0;

        virtual void
        async_write_some(
            const buffers::const_buffer_subspan&,
            asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

        virtual void
        async_read_some(
            const buffers::mutable_buffer_subspan&,
            asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

        virtual void async_shutdown(
            asio::any_completion_handler<void(error_code)>) = 0;

        virtual ~base() = default;
    };

    std::unique_ptr<base> stream_;
    asio::steady_timer rd_timer;
    asio::steady_timer wr_timer;
    std::size_t rd_remain_ = (std::numeric_limits<std::size_t>::max)();
    std::size_t wr_remain_ = (std::numeric_limits<std::size_t>::max)();
    std::size_t rd_limit_  = (std::numeric_limits<std::size_t>::max)();
    std::size_t wr_limit_  = (std::numeric_limits<std::size_t>::max)();
};

#endif
