//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//

#ifndef BOOST_HTTP_IO_IMPL_READ_HPP
#define BOOST_HTTP_IO_IMPL_READ_HPP

#include <boost/http_io/detail/except.hpp>
#include <boost/http_proto/error.hpp>
#include <boost/http_proto/parser.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace http_io {

namespace detail {

template<class AsyncStream>
class read_header_op
    : public asio::coroutine
{
    AsyncStream& stream_;
    http_proto::parser& pr_;
    std::size_t total_bytes_ = 0;

public:
    read_header_op(
        AsyncStream& s,
        http_proto::parser& pr) noexcept
        : stream_(s)
        , pr_(pr)
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
            if(pr_.got_header())
            {
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "immediate"));
                    asio::async_immediate(
                        self.get_io_executor(), std::move(self));
                }
                goto upcall;
            }
            for(;;)
            {
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "async_read_some"));
                    stream_.async_read_some(
                        pr_.prepare(),
                        std::move(self));
                }
                pr_.commit(bytes_transferred);
                total_bytes_ += bytes_transferred;
                if(ec == asio::error::eof)
                {
                    BOOST_ASSERT(
                        bytes_transferred == 0);
                    pr_.commit_eof();
                    ec = {};
                }
                else if(ec.failed())
                {
                    goto upcall;
                }
                pr_.parse(ec);
                if(ec != http_proto::condition::need_more_input)
                    break;
                if(pr_.got_header())
                {
                    ec = {}; // override possible need_more_input
                    break;
                }
            }

        upcall:
            self.complete(ec, total_bytes_);
        }
    }
};

//------------------------------------------------

template<class AsyncStream>
class read_body_op
    : public asio::coroutine
{
    AsyncStream& stream_;
    http_proto::parser& pr_;
    std::size_t total_bytes_ = 0;
    bool some_;

public:
    read_body_op(
        AsyncStream& s,
        http_proto::parser& pr,
        bool some)
        : stream_(s)
        , pr_(pr)
        , some_(some)
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
            pr_.parse(ec);
            if(ec != http_proto::condition::need_more_input)
            {
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "immediate"));
                    asio::async_immediate(
                        self.get_io_executor(), asio::append(std::move(self), ec));
                }
                goto upcall;
            }
            for(;;)
            {
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "async_read_some"));
                    stream_.async_read_some(
                        pr_.prepare(),
                        std::move(self));
                }
                pr_.commit(bytes_transferred);
                total_bytes_ += bytes_transferred;
                if(ec == asio::error::eof)
                {
                    BOOST_ASSERT(
                        bytes_transferred == 0);
                    pr_.commit_eof();
                    ec = {};
                }
                else if(ec.failed())
                {
                    goto upcall;
                }
                pr_.parse(ec);
                if(! ec.failed())
                {
                    BOOST_ASSERT(
                        pr_.is_complete());
                    break;
                }
                if(ec != http_proto::condition::need_more_input)
                    break;
                if(some_)
                {
                    ec = {};
                    break;
                }
            }

        upcall:
            self.complete(ec, total_bytes_);
        }
    }
};

} // detail

//------------------------------------------------

template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read_header(
    AsyncReadStream& s,
    http_proto::parser& pr,
    CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::read_header_op<
                AsyncReadStream>{s, pr},
            token,
            s);
}

template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read_some(
    AsyncReadStream& s,
    http_proto::parser& pr,
    CompletionToken&& token)
{
    // header must be read first!
    if(! pr.got_header())
        detail::throw_logic_error();

    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::read_body_op<
                AsyncReadStream>{s, pr, true},
            token,
            s);
}

template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read(
    AsyncReadStream& s,
    http_proto::parser& pr,
    CompletionToken&& token)
{
    // header must be read first!
    if(! pr.got_header())
        detail::throw_logic_error();

    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::read_body_op<
                AsyncReadStream>{s, pr, false},
            token,
            s);
}

} // http_io
} // boost

#endif
