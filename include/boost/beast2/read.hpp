//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_READ_HPP
#define BOOST_BEAST2_READ_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response_parser.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

namespace boost {
namespace beast2 {

/** Read a complete header from the stream.

    This function is used to asynchronously read a complete
    message header from a stream into an instance of parser.
    The function call always returns immediately. The
    asynchronous operation will continue until one of the
    following conditions is true:

    @li The parser reads the header section of the message.
    @li An error occurs.

    This operation is implemented in terms of zero or more
    calls to the stream's `async_read_some` function, and is
    known as a <em>composed operation</em>. The program must
    ensure that the stream performs no other reads until
    this operation completes.

    @param stream The stream from which the data is to be
    read. The type must meet the <em>AsyncReadStream</em>
    requirements.

    @param parser The parser to use. The object must remain
    valid at least until the handler is called; ownership is
    not transferred.

    @param token The completion token that will be used to
    produce a completion handler, which will be called when
    the read completes. The function signature of the
    completion handler must be:
    @code
    void handler(
        error_code const& error,        // result of operation
        std::size_t bytes_transferred   // the number of bytes consumed by the parser
    );
    @endcode
    Regardless of whether the asynchronous operation
    completes immediately or not, the completion handler
    will not be invoked from within this function. On
    immediate completion, invocation of the handler will be
    performed in a manner equivalent to using
    `asio::async_immediate`.

    @par Per-Operation Cancellation

    This asynchronous operation supports cancellation for
    the following `asio::cancellation_type` values:

    @li `asio::cancellation_type::terminal`
    @li `asio::cancellation_type::partial`
    @li `asio::cancellation_type::total`

    if they are also supported by the AsyncReadStream type's
    async_read_some operation.
*/
template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(
                typename AsyncReadStream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read_header(
    AsyncReadStream& s,
    http_proto::parser& pr,
    CompletionToken&& token
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(
            typename AsyncReadStream::executor_type));

/** Read part of the message body from the stream.

    This function is used to asynchronously read part of a
    message body from a stream into an instance of parser.
    The function call always returns immediately. The
    asynchronous operation will continue until one of the
    following conditions is true:

    @li The parser reads the header section of the message.
    @li The parser reads the entire message.
    @li Additional body data becomes available.
    @li An error occurs.

    This operation is implemented in terms of zero or more
    calls to the stream's `async_read_some` function, and is
    known as a <em>composed operation</em>. The program must
    ensure that the stream performs no other reads until
    this operation completes.

    @param stream The stream from which the data is to be
    read. The type must meet the <em>AsyncReadStream</em>
    requirements.

    @param parser The parser to use. The object must remain
    valid at least until the handler is called; ownership is
    not transferred.

    @param token The completion token that will be used to
    produce a completion handler, which will be called when
    the read completes. The function signature of the
    completion handler must be:
    @code
    void handler(
        error_code const& error,        // result of operation
        std::size_t bytes_transferred   // the number of bytes consumed by the parser
    );
    @endcode
    Regardless of whether the asynchronous operation
    completes immediately or not, the completion handler
    will not be invoked from within this function. On
    immediate completion, invocation of the handler will be
    performed in a manner equivalent to using
    `asio::async_immediate`.

    @par Per-Operation Cancellation

    This asynchronous operation supports cancellation for
    the following `asio::cancellation_type` values:

    @li `asio::cancellation_type::terminal`
    @li `asio::cancellation_type::partial`
    @li `asio::cancellation_type::total`

    if they are also supported by the AsyncReadStream type's
    async_read_some operation.
*/
template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(
                typename AsyncReadStream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read_some(
    AsyncReadStream& s,
    http_proto::parser& pr,
    CompletionToken&& token
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(
            typename AsyncReadStream::executor_type));

/** Read a complete message from the stream.

    This function is used to asynchronously read a complete
    message from a stream into an instance of parser.
    The function call always returns immediately. The
    asynchronous operation will continue until one of the
    following conditions is true:

    @li The parser reads the entire message.
    @li An error occurs.

    This operation is implemented in terms of zero or more
    calls to the stream's `async_read_some` function, and is
    known as a <em>composed operation</em>. The program must
    ensure that the stream performs no other reads until
    this operation completes.

    @param stream The stream from which the data is to be
    read. The type must meet the <em>AsyncReadStream</em>
    requirements.

    @param parser The parser to use. The object must remain
    valid at least until the handler is called; ownership is
    not transferred.

    @param token The completion token that will be used to
    produce a completion handler, which will be called when
    the read completes. The function signature of the
    completion handler must be:
    @code
    void handler(
        error_code const& error,        // result of operation
        std::size_t bytes_transferred   // the number of bytes consumed by the parser
    );
    @endcode
    Regardless of whether the asynchronous operation
    completes immediately or not, the completion handler
    will not be invoked from within this function. On
    immediate completion, invocation of the handler will be
    performed in a manner equivalent to using
    `asio::async_immediate`.

    @par Per-Operation Cancellation

    This asynchronous operation supports cancellation for
    the following `asio::cancellation_type` values:

    @li `asio::cancellation_type::terminal`
    @li `asio::cancellation_type::partial`
    @li `asio::cancellation_type::total`

    if they are also supported by the AsyncReadStream type's
    async_read_some operation.
*/
template<
    class AsyncReadStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(
                typename AsyncReadStream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_read(
    AsyncReadStream& s,
    http_proto::parser& pr,
    CompletionToken&& token
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(
            typename AsyncReadStream::executor_type));

} // beast2
} // boost

#include <boost/beast2/impl/read.hpp>

#endif
