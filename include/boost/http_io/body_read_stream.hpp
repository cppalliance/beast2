//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//

#ifndef BOOST_HTTP_IO_BODY_READ_STREAM_HPP
#define BOOST_HTTP_IO_BODY_READ_STREAM_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response_parser.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <cstdint>

namespace boost {
namespace http_io {

template<class UnderlyingAsyncReadStream>
class body_read_stream {

public:
    explicit
        body_read_stream(
            UnderlyingAsyncReadStream& und_stream,
            http_proto::parser& pr);

    template<
        class MutableBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(
            void(system::error_code, std::size_t)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
        void(system::error_code, std::size_t))
    async_read_some(
            const MutableBufferSequence& mb,
            CompletionToken&& token);

private:
    UnderlyingAsyncReadStream& und_stream_;
    http_proto::parser& pr_;
};

} // http_io
} // boost

#include <boost/http_io/impl/body_read_stream.hpp>

#endif
