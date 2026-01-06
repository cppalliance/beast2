//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_IMPL_READ_HPP
#define BOOST_BEAST2_IMPL_READ_HPP

#include <boost/beast2/detail/except.hpp>
#include <boost/http/error.hpp>
#include <boost/http/parser.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace beast2 {

namespace detail {

template<class AsyncStream>
class read_until_op
    : public asio::coroutine
{
    AsyncStream& stream_;
    http::parser& pr_;
    std::size_t total_bytes_ = 0;
    bool (&condition_)(http::parser&);

public:
    read_until_op(
        AsyncStream& s,
        http::parser& pr,
        bool (&condition)(http::parser&)) noexcept
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
                    if(ec == http::condition::need_more_input)
                    {
                        if(!!self.cancelled())
                        {
                            ec = asio::error::operation_aborted;
                            goto upcall;
                        }
                        // specific to http_io::async_read_some
                        if(total_bytes_ != 0 && condition_(pr_))
                        {
                            ec = {};
                            goto upcall;
                        }
                        break;
                    }
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
got_header_condition(http::parser& pr)
{
    return pr.got_header();
}

inline
bool
is_complete_condition(http::parser& pr)
{
    return pr.is_complete();
}

} // detail

//------------------------------------------------

/** Asynchronously reads some data into the parser.

    This function is used to asynchronously read data from a
    stream into the parser's input sequence. This function will always
    keep reading until a complete header is obtained.
    The function call will invoke the completion token
    with the following signature:
    @code
    void(system::error_code ec
    std::size_t bytes_transferred);
    @endcode
    @note The parser's input sequence may contain additional data
    beyond what was required to complete the header.
    @param s The stream to read from.
    @param pr The parser to read data into.
    @param token The completion token.
*/
template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read_some(
    AsyncReadStream& s,
    http::parser& pr,
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

/** Asynchronously reads data into the parser until the header is complete.
    This function is used to asynchronously read data from a
    stream into the parser's input sequence until the parser's
    header is complete.
    The function call will invoke the completion token
    with the following signature:
    @code
    void(system::error_code ec
    std::size_t bytes_transferred);
    @endcode
    @note The parser's input sequence may contain additional data
    beyond what was required to complete the header.
    @param s The stream to read from.
    @param pr The parser to read data into.
    @param token The completion token.
*/
template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read_header(
    AsyncReadStream& s,
    http::parser& pr,
    CompletionToken&& token)
{
    // TODO: async_read_header should not perform a read
    // operation if `parser::got_header() == true`.
    return async_read_some(s, pr, std::move(token));
}

/** Asynchronously reads data into the parser until the message is complete.
    This function is used to asynchronously read data from a
    stream into the parser's input sequence until the parser's
    message is complete.
    The function call will invoke the completion token
    with the following signature:
    @code
    void(system::error_code ec
    std::size_t bytes_transferred);
    @endcode
    @note The parser's input sequence may contain additional data
    beyond what was required to complete the message.
    @param s The stream to read from.
    @param pr The parser to read data into.
    @param token The completion token.
*/
template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read(
    AsyncReadStream& s,
    http::parser& pr,
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

} // beast2
} // boost

#endif
