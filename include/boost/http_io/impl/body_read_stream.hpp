//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//

#ifndef BOOST_HTTP_IO_IMPL_BODY_READ_STREAM_HPP
#define BOOST_HTTP_IO_IMPL_BODY_READ_STREAM_HPP

#include <boost/http_io/detail/except.hpp>
#include <boost/http_proto/error.hpp>
#include <boost/http_proto/parser.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace http_io {

namespace detail {

template <class MutableBufferSequence, class UnderlyingAsyncReadStream>
class body_read_stream_op : public asio::coroutine {

    UnderlyingAsyncReadStream& underlying_stream_;
    const MutableBufferSequence& mb_;
    http_proto::parser& pr_;
    bool already_have_header_ = false;
    bool some_ = false;

public:
    body_read_stream_op(
        UnderlyingAsyncReadStream& s,
        const MutableBufferSequence& mb,
        http_proto::parser& pr,
        bool some) noexcept
        : underlying_stream_(s)
        , mb_(mb)
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
            if (!pr_.is_complete())
            {
                for (;;)
                {
                    BOOST_ASIO_CORO_YIELD
                    {
                        BOOST_ASIO_HANDLER_LOCATION((
                            __FILE__, __LINE__,
                            "async_read_some"));
                        underlying_stream_.async_read_some(
                            pr_.prepare(),
                            std::move(self));
                    }
                    pr_.commit(bytes_transferred);
                    if (ec == asio::error::eof)
                    {
                        BOOST_ASSERT(
                            bytes_transferred == 0);
                        pr_.commit_eof();
                        ec = {};
                    }
                    else if (ec.failed())
                    {
                        break; // genuine error
                    }
                    pr_.parse(ec);
                    if (ec.failed() && ec != http_proto::condition::need_more_input)
                    {
                        break; // genuine error.
                    }
                    if (already_have_header_) {
                        if (!ec.failed())
                        {
                            BOOST_ASSERT(
                                pr_.is_complete());
                            break;
                        }
                        if (some_)
                        {
                            ec = {};
                            break;
                        }
                    }
                    if (!already_have_header_ && pr_.got_header())
                    {
                        already_have_header_ = true;
                        ec = {}; // override possible need_more_input
                        pr_.parse(ec); // having parsed the header, callle parse again for the start of the body.
                        if (ec.failed() && ec != http_proto::condition::need_more_input)
                        {
                            break; // genuine error.
                        }
                        if (!ec.failed())
                        {
                            BOOST_ASSERT(
                                pr_.is_complete());
                            break;
                        }
                    }
                }
            }

            auto source_buf = pr_.pull_body();

            std::size_t n = boost::asio::buffer_copy(mb_, source_buf);

            pr_.consume_body(n);

            ec = (n != 0) ? system::error_code{} : asio::stream_errc::eof;

            // for some reason this crashes - not sure yet why:
            //asio::dispatch(
            //    asio::get_associated_executor(underlying_stream_), 
            //    asio::prepend(std::move(self), ec, n));

            self.complete(ec, n); // TODO - work out the byte count
        }
    }
};

} // detail

//------------------------------------------------

    // TODO: copy in Beast's stream traits to check if UnderlyingAsyncReadStream
    // is an AsyncReadStream, and also static_assert that body_read_stream is too.



template<class UnderlyingAsyncReadStream>
body_read_stream<UnderlyingAsyncReadStream>::body_read_stream(
      const rts::context& rts_ctx
    , UnderlyingAsyncReadStream& und_stream
    , http_proto::parser& pr)
    :
      rts_ctx_(rts_ctx)
    , und_stream_(und_stream)
    , pr_(pr)
{
}


template<class UnderlyingAsyncReadStream>
template<
    class MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void(system::error_code, std::size_t))
body_read_stream<UnderlyingAsyncReadStream>::async_read_some(
      const MutableBufferSequence& mb
    , CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(system::error_code, std::size_t)>(
            detail::body_read_stream_op<
            MutableBufferSequence, UnderlyingAsyncReadStream>{und_stream_, mb, pr_, true},
            token,
            asio::get_associated_executor(und_stream_)
        );
}

} // http_io
} // boost

#endif
