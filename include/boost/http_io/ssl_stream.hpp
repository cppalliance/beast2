//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SSL_SOCKET_HPP
#define BOOST_HTTP_IO_SSL_SOCKET_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <type_traits>

namespace boost {
namespace http_io {

/** A stream which can be either plain or SSL
*/
template<class AsyncStream>
class ssl_stream
{
    struct initiate_async_read_some;
    struct initiate_async_write_some;

public:
    using ssl_stream_type = asio::ssl::stream<AsyncStream>;

    using next_layer_type = typename ssl_stream_type::next_layer_type;

    using lowest_layer_type = typename ssl_stream_type::lowest_layer_type;

    using executor_type = typename ssl_stream_type::executor_type;

    /** Destructor

        @note A @c stream object must not be destroyed while there are
        pending asynchronous operations associated with it.
    */
    ~ssl_stream() = default;

    ssl_stream(ssl_stream&&) = default;

    ssl_stream& operator=(ssl_stream&& other) = default;

    template<class... Args>
    explicit
    ssl_stream(Args&&... args)
        : stream_(std::forward<Args>(args)...)
    {
    }

    executor_type get_executor() noexcept
    {
        return stream_.get_executor();
    }

    next_layer_type& next_layer()
    {
        return stream_.next_layer();
    }

    next_layer_type const& next_layer() const
    {
        return stream_.next_layer();
    }

    lowest_layer_type& lowest_layer()
    {
        return stream_.lowest_layer();
    }

    lowest_layer_type const& lowest_layer() const
    {
        return stream_.lowest_layer();
    }

    ssl_stream_type& stream()
    {
        return stream_;
    }

    ssl_stream_type const& stream() const
    {
        return stream_;
    }

    void set_ssl(bool ssl) noexcept
    {
        is_ssl_ = ssl;
    }

    bool is_ssl() const noexcept
    {
        return is_ssl_;
    }

    template <typename MutableBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void (system::error_code,
            std::size_t)) ReadToken = asio::default_completion_token_t<executor_type>>
    auto async_read_some(MutableBufferSequence const& buffers,
        ReadToken&& token = asio::default_completion_token_t<executor_type>()) ->
            decltype(asio::async_initiate<ReadToken,
                void (system::error_code, std::size_t)>(
                    std::declval<initiate_async_read_some>(), token, buffers))
    {
        return asio::async_initiate<ReadToken,
            void (system::error_code, std::size_t)>(
                initiate_async_read_some(this), token, buffers);
    }

    template <typename ConstBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
            std::size_t)) WriteToken = asio::default_completion_token_t<executor_type>>
    auto async_write_some(ConstBufferSequence const& buffers,
        WriteToken&& token = asio::default_completion_token_t<executor_type>()) ->
            decltype(asio::async_initiate<WriteToken,
                void (system::error_code, std::size_t)>(
                    std::declval<initiate_async_write_some>(), token, buffers))
    {
        return asio::async_initiate<WriteToken,
            void (system::error_code, std::size_t)>(
                initiate_async_write_some(this), token, buffers);
    }

private:
    struct initiate_async_read_some
    {
        using executor_type = typename ssl_stream::executor_type;

        explicit initiate_async_read_some(
            ssl_stream* self) noexcept
            : self_(self)
        {
        }

        executor_type get_executor() const noexcept
        {
            return self_->get_executor();
        }

        template <class ReadHandler, class MutableBufferSequence>
        void operator()(ReadHandler&& handler,
            MutableBufferSequence const& buffers) const
        {
            if(self_->is_ssl_)
                self_->stream_.async_read_some(buffers,
                    std::forward<ReadHandler>(handler));
            else
                self_->next_layer().async_read_some(buffers,
                    std::forward<ReadHandler>(handler));
        }

        ssl_stream* self_;
    };

    struct initiate_async_write_some
    {
        using executor_type = typename ssl_stream::executor_type;

        explicit initiate_async_write_some(
            ssl_stream* self) noexcept
            : self_(self)
        {
        }

        executor_type get_executor() const noexcept
        {
            return self_->get_executor();
        }

        template <class WriteHandler, class ConstBufferSequence>
        void operator()(WriteHandler&& handler,
            ConstBufferSequence const& buffers) const
        {
            if(self_->is_ssl_)
                self_->stream_.async_write_some(buffers,
                    std::forward<WriteHandler>(handler));
            else
                self_->next_layer().async_write_some(buffers,
                    std::forward<WriteHandler>(handler));
        }

        ssl_stream* self_;
    };

    ssl_stream_type stream_;
    bool is_ssl_ = false;
};

} // http_io
} // boost

#endif
