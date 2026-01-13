//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_IMPL_BODY_WRITE_STREAM_HPP
#define BOOST_BEAST2_IMPL_BODY_WRITE_STREAM_HPP

#include <boost/beast2/write.hpp>

#include <boost/assert.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/coroutine.hpp>

namespace boost {
namespace beast2 {

namespace detail {

template<class AsyncWriteStream>
class body_write_stream_close_op : public asio::coroutine
{
    body_write_stream<AsyncWriteStream>& bws_;

public:
    body_write_stream_close_op(
        body_write_stream<AsyncWriteStream>& bws) noexcept
        : bws_(bws)
    {
    }

    template<class Self>
    void
    operator()(
        Self& self,
        system::error_code ec = {},
        std::size_t = 0)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            self.reset_cancellation_state(asio::enable_total_cancellation());

            // Check for a saved error from a previous async_write_some call.
            if(bws_.ec_.failed())
            {
                ec = bws_.ec_;
                bws_.ec_ = {};
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION(
                        (__FILE__, __LINE__, "immediate"));
                    auto io_ex = self.get_io_executor();
                    asio::async_immediate(
                        io_ex,
                        asio::append(std::move(self), ec));
                }
                goto upcall;
            }

            bws_.srs_.close();

            BOOST_ASIO_CORO_YIELD
            {
                BOOST_ASIO_HANDLER_LOCATION(
                    (__FILE__, __LINE__, "async_write_some"));
                beast2::async_write(bws_.stream_, bws_.sr_, std::move(self));
            }

        upcall:
            self.complete(ec);
        }
    }
};

template<class ConstBufferSequence, class AsyncWriteStream>
class body_write_stream_op : public asio::coroutine
{
    body_write_stream<AsyncWriteStream>& bws_;
    ConstBufferSequence cb_;
    std::size_t bytes_;

public:
    body_write_stream_op(
        body_write_stream<AsyncWriteStream>& bws,
        ConstBufferSequence const& cb) noexcept
        : bws_(bws)
        , cb_(cb)
        , bytes_(0)
    {
    }

    template<class Self>
    void
    operator()(
        Self& self,
        system::error_code ec = {},
        std::size_t = 0)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Verify preconditions
            BOOST_ASSERT(!bws_.sr_.is_done());

            self.reset_cancellation_state(asio::enable_total_cancellation());

            // A zero-sized buffer is a special case, we are required to
            // complete immediately with no error. Also check for a saved
            // error from a previous call.
            if(bws_.ec_.failed() ||
               capy::buffer_size(cb_) == 0)
            {
                ec = bws_.ec_;
                bws_.ec_ = {};
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION(
                        (__FILE__, __LINE__, "immediate"));
                    auto io_ex = self.get_io_executor();
                    asio::async_immediate(
                        io_ex,
                        asio::append(std::move(self), ec));
                }
                goto upcall;
            }

            // The serializer's internal buffer may be full, so we may have no
            // option but to try to write to the stream to clear space.
            // This may require multiple attempts as buffer space cannot
            // be cleared until the headers have been written.
            for(;;)
            {
                bytes_ = asio::buffer_copy(bws_.srs_.prepare(), cb_);
                bws_.srs_.commit(bytes_);

                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION(
                        (__FILE__, __LINE__, "async_write_some"));
                    async_write_some(bws_.stream_, bws_.sr_, std::move(self));
                }

                if(ec.failed())
                {
                    if(bytes_ != 0)
                    {
                        bws_.ec_ = ec;
                        ec = {};
                    }
                    break;
                }

                if(bytes_ != 0)
                    break;

                if(!!self.cancelled())
                {
                    ec = asio::error::operation_aborted;
                    break;
                }
            }

        upcall:
            self.complete(ec, bytes_);
        }
    }
};

} // detail

//------------------------------------------------

// TODO: copy in Beast's stream traits to check if AsyncWriteStream
// is an AsyncWriteStream, and also static_assert that body_write_stream is too.

template<class AsyncWriteStream>
body_write_stream<AsyncWriteStream>::
body_write_stream(
    AsyncWriteStream& s,
    http::serializer& sr,
    http::serializer::stream srs)
    : stream_(s)
    , sr_(sr)
    , srs_(std::move(srs))
{
    // Verify preconditions
    BOOST_ASSERT(srs_.is_open());
}

template<class AsyncWriteStream>
template<
    class ConstBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, std::size_t))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(system::error_code, std::size_t))
body_write_stream<AsyncWriteStream>::
async_write_some(
    ConstBufferSequence const& cb,
    CompletionToken&& token)
{
    return asio::
        async_compose<CompletionToken, void(system::error_code, std::size_t)>(
            detail::body_write_stream_op<ConstBufferSequence, AsyncWriteStream>{
                *this, cb },
            token,
            stream_);
}

template<class AsyncWriteStream>
template<
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(system::error_code))
body_write_stream<AsyncWriteStream>::
async_close(
    CompletionToken&& token)
{
    return asio::
        async_compose<CompletionToken, void(system::error_code)>(
            detail::body_write_stream_close_op<AsyncWriteStream>{ *this },
            token,
            stream_);
}

} // beast2
} // boost

#endif // BOOST_BEAST2_IMPL_BODY_WRITE_STREAM_HPP
