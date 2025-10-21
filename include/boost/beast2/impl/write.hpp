//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_IMPL_WRITE_HPP
#define BOOST_BEAST2_IMPL_WRITE_HPP

#include <boost/asio/append.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <boost/http_proto/serializer.hpp>

namespace boost {
namespace beast2 {

namespace detail {

template<class WriteStream>
class write_some_op
    : public asio::coroutine
{
    using buffers_type =
        http_proto::serializer::const_buffers_type;

    WriteStream& dest_;
    http_proto::serializer& sr_;

public:
    write_some_op(
        WriteStream& dest,
        http_proto::serializer& sr) noexcept
        : dest_(dest)
        , sr_(sr)
    {
    }

    template<class Self>
    void
    operator()(
        Self& self,
        system::error_code ec = {},
        std::size_t bytes_transferred = {})
    {
        system::result<buffers_type> rv;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            rv = sr_.prepare();
            if(! rv)
            {
                ec = rv.error();
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "immediate"));
                    auto io_ex = self.get_io_executor();
                    asio::async_immediate(
                        io_ex,
                        asio::append(std::move(self), ec));
                }
                goto upcall;
            }

            BOOST_ASIO_CORO_YIELD
            {
                BOOST_ASIO_HANDLER_LOCATION((
                    __FILE__, __LINE__,
                    "beast2::write_some_op"));
                dest_.async_write_some(
                    *rv,
                    std::move(self));
            }
            sr_.consume(bytes_transferred);

        upcall:
            self.complete(
                ec, bytes_transferred );
        }
    }
};

//------------------------------------------------

template<class WriteStream>
class write_op
    : public asio::coroutine
{
    WriteStream& dest_;
    http_proto::serializer& sr_;
    std::size_t n_ = 0;

public:
    write_op(
        WriteStream& dest,
        http_proto::serializer& sr) noexcept
        : dest_(dest)
        , sr_(sr)
    {
    }

    template<class Self>
    void
    operator()(
        Self& self,
        system::error_code ec = {},
        std::size_t bytes_transferred = 0)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            do
            {
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "beast2::write_op"));
                    async_write_some(
                        dest_, sr_, std::move(self));
                }
                n_ += bytes_transferred;
                if(ec.failed())
                    break;
            }
            while(! sr_.is_done());

            // upcall
            self.complete(ec, n_ );
        }
    }
};

//------------------------------------------------

#if 0
template<
    class WriteStream,
    class ReadStream,
    class CompletionCondition>
class relay_some_op
    : public asio::coroutine
{
    WriteStream& dest_;
    ReadStream& src_;
    CompletionCondition cond_;
    http_proto::serializer& sr_;
    std::size_t bytes_read_ = 0;

public:
    relay_some_op(
        WriteStream& dest,
        ReadStream& src,
        CompletionCondition const& cond,
        http_proto::serializer& sr) noexcept
        : dest_(dest)
        , src_(src)
        , cond_(cond)
        , sr_(sr)
    {
    }

    template<class Self>
    void
    operator()(
        Self& self,
        system::error_code ec = {},
        std::size_t bytes_transferred = 0)
    {
        urls::result<
            http_proto::serializer::buffers> rv;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Nothing to do
            BOOST_ASSERT(! sr_.is_complete());

            rv = sr_.prepare();
            if(! rv)
            {
                ec = rv.error();
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "beast2::relay_some_op"));
                    asio::post(std::move(self));
                }
                goto upcall;
            }

            BOOST_ASIO_CORO_YIELD
            {
                BOOST_ASIO_HANDLER_LOCATION((
                    __FILE__, __LINE__,
                    "beast2::relay_some_op"));
                dest_.async_write_some(
                    write_buffers(*rv),
                    std::move(self));
            }
            sr_.consume(bytes_transferred);

        upcall:
            self.complete(
                ec, bytes_transferred );
        }
    }
};
#endif

} // detail

//------------------------------------------------

template<
    class AsyncWriteStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_write_some(
    AsyncWriteStream& dest,
    http_proto::serializer& sr,
    CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::write_some_op<
                AsyncWriteStream>{dest, sr},
            token, dest);
}

template<
    class AsyncWriteStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_write(
    AsyncWriteStream& dest,
    http_proto::serializer& sr,
    CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::write_op<
                AsyncWriteStream>{dest, sr},
            token,
            dest);
}

#if 0
template<
    class AsyncWriteStream,
    class AsyncReadStream,
    class CompletionCondition,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_relay_some(
    AsyncWriteStream& dest,
    AsyncReadStream& src,
    CompletionCondition const& cond,
    http_proto::serializer& sr,
    CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::relay_some_op<
                AsyncWriteStream,
                AsyncReadStream,
                CompletionCondition>{
                    dest, src, cond, sr},
            token,
            dest,
            src);
}
#endif

} // beast2
} // boost

#endif
