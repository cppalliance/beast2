//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_BODY_WRITE_STREAM_HPP
#define BOOST_BEAST2_BODY_WRITE_STREAM_HPP

#include <boost/asio/async_result.hpp>
#include <boost/http/serializer.hpp>
#include <boost/system/error_code.hpp>

#include <utility>

namespace boost {
namespace beast2 {

namespace detail {

template<class, class>
class body_write_stream_op;

template<class>
class body_write_stream_close_op;

} // detail

/** A body writer for HTTP/1 messages.

    This type is modelled on asio's AsyncWriteStream, and is constructed with a
    reference to an underlying AsyncWriteStream.

    Any call to `async_write_some` initially triggers writes to the underlying
    stream until all of the HTTP headers and at least one byte of the body have
    been written and processed.  Thereafter, each subsequent call to
    `async_write_some` processes at least one byte of the body, triggering, where
    required, calls to the underlying stream's `async_write_some` method.  The
    body data is read from the referenced ConstBufferSequence.

    All processing depends on a beast2::serializer object owned by the caller and
    referenced in the construction of this object.

    @par Deviations from AsyncWriteStream

    This type deviates from the strict AsyncWriteStream requirements in the
    following ways:

    @li <b>Deferred error reporting:</b> If an error or cancellation occurs
        after data has been successfully committed to the serializer, the
        operation completes with success and reports the number of bytes
        consumed. The error is saved and reported on the next call to
        `async_write_some`. This differs from the AsyncWriteStream requirement
        that on error, `bytes_transferred` must be 0. This behaviour ensures
        that the caller knows exactly how many bytes were consumed by the
        serializer, preventing data loss or duplication.

    @see
        @ref http::serializer.
*/
template<class AsyncWriteStream>
class body_write_stream
{

public:
    /** The type of the executor associated with the stream.

        This will be the type of executor used to invoke completion handlers
        which do not have an explicit associated executor.
    */
    using executor_type =
        decltype(std::declval<AsyncWriteStream&>().get_executor());

    /** Return the executor associated with the object.

        This function may be used to obtain the executor object that the stream
        uses to dispatch completion handlers without an associated executor.

        @return A copy of the executor that stream will use to dispatch
        handlers.
    */
    executor_type
    get_executor()
    {
        return stream_.get_executor();
    }

    /** Constructor

        This constructor creates the stream which retains a reference to the
        underlying stream.  The underlying stream then needs to be open before
        data can be written

        @param s  The underlying stream to which the HTTP message is written.
                  This object's executor is initialized to that of the
                  underlying stream.

        @param sr A http::serializer object which will perform the serialization of
                  the HTTP message and extraction of the body.  This must be
                  initialized by the caller and ownership of the serializer is
                  retained by the caller, which must guarantee that it remains
                  valid until the handler is called.

        @param srs A http::serializer::stream object which must have been
                   obtained by a call to `start_stream` on `sr`.  Ownership of
                   the serializer::stream is transferred to this object.
    */
    explicit body_write_stream(
        AsyncWriteStream& s,
        http::serializer& sr,
        http::serializer::stream srs);

    /** Write some data asynchronously.

        This function is used to asynchronously write data to the stream.

        This call always returns immediately.  The asynchronous operation will
        continue until one of the following conditions is true:

        @li One or more bytes are written from `cb` to the body stored in the
            serializer and one or more bytes are written from the serializer to the
            underlying stream.

        @li An error occurs.

        The algorithm, known as a <em>composed asynchronous operation</em>, is
        implemented in terms of calls to the underlying stream's
        `async_write_some` function. The program must ensure that no other calls
        implemented using the underlying stream's `async_write_some` are
        performed until this operation completes.

        @param cb The buffer sequence from which the body data will be read.  If
        the size of the buffer sequence is zero bytes, the operation always
        completes immediately with no error.  Although the buffers object may be
        copied as necessary, ownership of the underlying memory blocks is
        retained by the caller, which must guarantee that they remain valid until
        the handler is called.  Where the internal buffer of the contained
        serializer is not of sufficient size to hold the data to be copied from
        cb, the remainder may be written by subsequent calls to this function.

        @param handler The completion handler to invoke when the operation
        completes.  The implementation takes ownership of the handler by
        performing a decay-copy.  The equivalent function signature of the
        handler must be:
        @code
        void handler(
            error_code const& error,        // result of operation
            std::size_t bytes_transferred   // the number of bytes consumed from
                                            // cb by the serializer
        );
        @endcode
        Regardless of whether the asynchronous operation
        completes immediately or not, the completion handler will not be invoked
        from within this function. On immediate completion, invocation of the
        handler will be performed in a manner equivalent to using
        `asio::async_immediate`.

        @note The `async_write_some` operation may not transmit all of the
        requested number of bytes.  Consider using the function
        `asio::async_write` if you need to ensure that the requested amount of
        data is written before the asynchronous operation completes.

        @note This function does not guarantee that all of the consumed data is
        written to the underlying stream. For this reason one or more calls to
        `async_write_some` must be followed by a call to `async_close` to put the
        serializer into the `done` state and to write all data remaining in the
        serializer to the underlying stream.

        @par Per-Operation Cancellation

        This asynchronous operation supports cancellation for the following
        net::cancellation_type values:

        @li @c net::cancellation_type::terminal
        @li @c net::cancellation_type::partial
        @li @c net::cancellation_type::total

        if they are also supported by the underlying stream's @c async_write_some
        operation.
    */
    template<
        class ConstBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, std::size_t))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
        CompletionToken,
        void(system::error_code, std::size_t))
    async_write_some(ConstBufferSequence const& cb, CompletionToken&& handler);

    /** Close serializer::stream and flush remaining data to the underlying stream asynchronously.

        This function is used to asynchronously call `close` on the
        `serializer::stream` object referenced in the construction of this
        `body_write_stream` and write all remaining data in the serializer to the
        underlying stream.

        This call always returns immediately.  The asynchronous operation will
        continue until one of the following conditions is true:

        @li All remaining output bytes of the serializer are written to the
            underlying stream and the serializer's `is_done()` method returns true.

        @li An error occurs.

        The algorithm, known as a <em>composed asynchronous operation</em>, is
        implemented in terms of calls to the underlying stream's
        `async_write_some` function. The program must ensure that no other calls
        implemented using the underlying stream's `async_write_some` are
        performed until this operation completes.

        @param handler The completion handler to invoke when the operation
        completes.  The implementation takes ownership of the handler by
        performing a decay-copy.  The equivalent function signature of the
        handler must be:
        @code
        void handler(
            error_code const& error         // result of operation
        );
        @endcode
        Regardless of whether the asynchronous operation
        completes immediately or not, the completion handler will not be invoked
        from within this function. On immediate completion, invocation of the
        handler will be performed in a manner equivalent to using
        `asio::async_immediate`.

        @par Per-Operation Cancellation

        This asynchronous operation supports cancellation for the following
        net::cancellation_type values:

        @li @c net::cancellation_type::terminal
        @li @c net::cancellation_type::partial
        @li @c net::cancellation_type::total

        if they are also supported by the underlying stream's @c async_write_some
        operation.
    */
    template<
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
        CompletionToken,
        void(system::error_code))
    async_close(CompletionToken&& handler);

private:
    template<class, class>
    friend class detail::body_write_stream_op;

    template<class>
    friend class detail::body_write_stream_close_op;

    AsyncWriteStream& stream_;
    http::serializer& sr_;
    http::serializer::stream srs_;
    system::error_code ec_;
};

} // beast2
} // boost

#include <boost/beast2/impl/body_write_stream.hpp>

#endif // BOOST_BEAST2_BODY_WRITE_STREAM_HPP
