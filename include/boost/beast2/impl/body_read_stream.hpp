//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/beast2
//

#ifndef BOOST_HTTP_IO_IMPL_BODY_READ_STREAM_HPP
#define BOOST_HTTP_IO_IMPL_BODY_READ_STREAM_HPP

#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/read.hpp>
#include <boost/http_proto/parser.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>

namespace boost {
namespace beast2 {

namespace detail {

template <class MutableBufferSequence, class AsyncReadStream>
class body_read_stream_op : public asio::coroutine {

    AsyncReadStream& us_;
    MutableBufferSequence mb_;
    http_proto::parser& pr_;
    bool some_ = false; // Whether this is a read_some request.
    std::size_t bb_ = 0;

public:

    body_read_stream_op(
        AsyncReadStream& s,
        MutableBufferSequence&& mb,
        http_proto::parser& pr,
        bool some) noexcept
        : us_(s)
        , mb_(std::move(mb))
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
        boost::ignore_unused(bytes_transferred);

        BOOST_ASIO_CORO_REENTER(*this)
        {
            if (!pr_.got_header())
            {
                BOOST_ASIO_CORO_YIELD
                {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "async_read_header"));
                    beast2::async_read_header<
                        AsyncReadStream,
                        Self  > (
                        us_,
                        pr_,
                        std::move(self));
                }
                if (ec.failed()) goto upcall;
            }

            BOOST_ASIO_CORO_YIELD
            {
                if (some_) {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "async_read_some"));
                    beast2::async_read_some(
                        us_,
                        pr_,
                        std::move(self));
                }
                else {
                    BOOST_ASIO_HANDLER_LOCATION((
                        __FILE__, __LINE__,
                        "async_read"));
                    beast2::async_read(
                        us_,
                        pr_,
                        std::move(self));
                }
            }

        upcall:
            std::size_t n = 0;

            if (!ec.failed())
            {
                auto source_buf = pr_.pull_body();

                n = boost::asio::buffer_copy(mb_, source_buf);

                pr_.consume_body(n);

                ec = (n != 0) ? system::error_code{} : asio::stream_errc::eof;
            }

            self.complete(ec, n);
        }
    }
};

} // detail

//------------------------------------------------

    // TODO: copy in Beast's stream traits to check if AsyncReadStream
    // is an AsyncReadStream, and also static_assert that body_read_stream is too.



template<class AsyncReadStream>
body_read_stream<AsyncReadStream>::body_read_stream(
      AsyncReadStream& und_stream
    , http_proto::parser& pr)
    :
      us_(und_stream)
    , pr_(pr)
{
}


template<class AsyncReadStream>
template<
    class MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void(system::error_code, std::size_t))
body_read_stream<AsyncReadStream>::async_read_some(
      MutableBufferSequence mb
    , CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::body_read_stream_op<
            MutableBufferSequence, AsyncReadStream>{us_, std::move(mb), pr_, true},
            token,
            us_
        );
}

template<class AsyncReadStream>
template<
    class MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void(system::error_code, std::size_t))
    body_read_stream<AsyncReadStream>::async_read(
          MutableBufferSequence mb
        , CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::body_read_stream_op<
            MutableBufferSequence, AsyncReadStream>{us_, std::move(mb), pr_, false},
            token,
            us_
        );
}

} // beast2
} // boost

#endif
