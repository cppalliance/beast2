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
class read_until_op
    : public asio::coroutine
{
    AsyncStream& stream_;
    http_proto::parser& pr_;
    std::size_t total_bytes_ = 0;
    bool (&condition_)(http_proto::parser&);

public:
    read_until_op(
        AsyncStream& s,
        http_proto::parser& pr,
        bool (&condition)(http_proto::parser&)) noexcept
        : stream_(s)
        , pr_(pr)
        , condition_(condition)
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
            self.reset_cancellation_state(
                asio::enable_total_cancellation());

            for(;;)
            {
                for(;;)
                {
                    pr_.parse(ec);
                    if(ec == http_proto::condition::need_more_input)
                        break;
                    if(ec.failed() || condition_(pr_))
                    {
                        if(total_bytes_ == 0)
                        {
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
                        }
                        goto upcall;
                    }
                }
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
            }

        upcall:
            self.complete(ec, total_bytes_);
        }
    }
};

inline
bool
got_header_condition(http_proto::parser& pr)
{
    return pr.got_header();
}

inline
bool
is_complete_condition(http_proto::parser& pr)
{
    return pr.is_complete();
}

} // detail

//------------------------------------------------

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
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::read_until_op<AsyncReadStream>
                {s, pr, detail::got_header_condition},
            token,
            s);
}

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
    // TODO: async_read_header should not perform a read
    // operation if `parser::got_header() == true`.
    return async_read_some(s, pr, std::move(token));
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
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::read_until_op<AsyncReadStream>
                {s, pr, detail::is_complete_condition},
            token,
            s);
}

} // http_io
} // boost

#endif
