//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_BODY_READ_STREAM_HPP
#define BOOST_BEAST2_BODY_READ_STREAM_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/http/parser.hpp>
#include <boost/system/error_code.hpp>

namespace boost {
namespace beast2 {

/** A body reader for HTTP/1 messages.

    This type meets the requirements of asio's AsyncReadStream, and is
    constructed with a reference to an underlying AsyncReadStream.

    Any call to `async_read_some` initially triggers reads from the underlying
    stream until all of the HTTP headers and at least one byte of the body have
    been read and processed.  Thereafter, each subsequent call to
    `async_read_some` processes at least one byte of the body, triggering, where
    required, calls to the underlying stream's `async_read_some` method.  The
    resulting body data is stored in the referenced MutableBufferSequence.

    All processing depends on a beast2::parser object owned by the caller and
    referenced in the construction of this object.

    @see
        @ref http::parser.
*/
template<class AsyncReadStream>
class body_read_stream
{

public:
    /** The type of the executor associated with the stream.

        This will be the type of executor used to invoke completion handlers
        which do not have an explicit associated executor.
    */
    typedef typename AsyncReadStream::executor_type executor_type;

    /** Get the executor associated with the object.

        This function may be used to obtain the executor object that the stream
        uses to dispatch completion handlers without an assocaited executor.

        @return A copy of the executor that stream will use to dispatch
        handlers.
    */
    executor_type
    get_executor()
    {
        return stream_.get_executor();
    }

    /** Constructor

        This constructor creates the stream by forwarding all arguments to the
        underlying socket.  The socket then needs to be open and connected or
        accepted before data can be sent or received on it.

        @param s  The underlying stream from which the HTTP message is read.
                  This object's executor is initialized to that of the
                  underlying stream.

        @param pr A http::parser object which will perform the parsing of
                  the HTTP message and extraction of the body.  This must be
                  initialized by the caller and ownership of the parser is
                  retained by the caller, which must guarantee that it remains
                  valid until the handler is called.
    */
    explicit body_read_stream(AsyncReadStream& s, http::parser& pr);

    /** Read some data asynchronously.

        This function is used to asynchronously read data from the stream.

        This call always returns immediately.  The asynchronous operation will
        continue until one of the following conditions is true:

        @li The HTTP headers are read in full from the underlying stream and one
            or more bytes of the body are read from the stream and stored in the
            buffer `mb`.

        @li An error occurs.

        The algorithm, known as a <em>composed asynchronous operation</em>, is
        implemented in terms of calls to the underlying stream's
        `async_read_some` function. The program must ensure that no other calls
        implemented using the underlying stream's `async_read_some` are
        performed until this operation completes.

        @param mb The buffers into which the body data will be read.  If the
        size of the buffers is zero bytes, the operation always completes
        immediately with no error.  Although the buffers object may be copied as
        necessary, ownership of the underlying memory blocks is retained by the
        caller, which must guarantee that they remain valid until the handler is
        called.  Where the mb buffer is not of sufficient size to hold the read
        data, the remainder may be read by subsequent calls to this function.

        @param handler The completion handler to invoke when the operation
        completes.  The implementation takes ownership of the handler by
        performing a decay-copy.  The equivalent function signature of the
        handler must be:
        @code
        void handler(
            error_code const& error,        // result of operation
            std::size_t bytes_transferred   // the number of bytes consumed by the parser
        );
        @endcode
        Regardless of whether the asynchronous operation
        completes immediately or not, the completion handler will not be invoked
        from within this function. On immediate completion, invocation of the
        handler will be performed in a manner equivalent to using
        `asio::async_immediate`.

        @note The `async_read_some` operation may not receive all of the
        requested number of bytes.  Consider using the function
        `asio::async_read` if you need to ensure that the requested amount of
        data is read before the asynchronous operation completes.

        @par Per-Operation Cancellation

        This asynchronous operation supports cancellation for the following
        net::cancellation_type values:

        @li @c net::cancellation_type::terminal
        @li @c net::cancellation_type::partial
        @li @c net::cancellation_type::total

        if they are also supported by the underlying stream's @c async_read_some
        operation.
    */
    template<
        class MutableBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, std::size_t))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
        CompletionToken,
        void(system::error_code, std::size_t))
    async_read_some(MutableBufferSequence mb, CompletionToken&& handler);

private:
    AsyncReadStream& stream_;
    http::parser& pr_;
};

} // beast2
} // boost

#include <boost/beast2/impl/body_read_stream.hpp>

#endif // BOOST_BEAST2_BODY_READ_STREAM_HPP
