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
#include <boost/asio/immediate.hpp>
#include <boost/http_proto/parser.hpp>
#include <boost/http_proto/serializer.hpp>

namespace asio       = boost::asio;
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
                const const_buffers_type& buffers,
                asio::any_completion_handler<void(error_code, std::size_t)>
                    handler) override
            {
                stream_.async_write_some(buffers, std::move(handler));
            }

            virtual void
            async_read_some(
                const mutable_buffers_type& buffers,
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
                    boost::asio::
                        async_compose<decltype(handler), void(error_code)>(
                            [this, init = false](
                                auto&& self, error_code ec = {}) mutable
                            {
                                if(std::exchange(init, true))
                                    return self.complete(ec);

                                stream_.close(ec);
                                asio::async_immediate(
                                    stream_.get_executor(),
                                    asio::append(std::move(self), ec));
                            },
                            handler,
                            get_executor());
                }
            }
        };

        stream_ = std::make_unique<impl>(std::move(stream));
    }

    executor_type
    get_executor() noexcept
    {
        return stream_->get_executor();
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_write_some(
        const const_buffers_type& buffers,
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::
            async_initiate<CompletionToken, void(error_code, std::size_t)>(
                [this](auto handler, const auto& buffers)
                { stream_->async_write_some(buffers, std::move(handler)); },
                token,
                buffers);
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_read_some(
        const mutable_buffers_type& buffers,
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::
            async_initiate<CompletionToken, void(error_code, std::size_t)>(
                [this](auto handler, const auto& buffers)
                { stream_->async_read_some(buffers, std::move(handler)); },
                token,
                buffers);
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_shutdown(
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::async_initiate<CompletionToken, void(error_code)>(
            [this](auto handler)
            { stream_->async_shutdown(std::move(handler)); },
            token);
    }

private:
    struct base
    {
        virtual asio::any_io_executor
        get_executor() = 0;

        virtual void
        async_write_some(
            const const_buffers_type&,
            asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

        virtual void
        async_read_some(
            const mutable_buffers_type&,
            asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

        virtual void async_shutdown(
            asio::any_completion_handler<void(error_code)>) = 0;

        virtual ~base()
        {
        }
    };

    std::unique_ptr<base> stream_;
};

#endif
