//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_WRITE_HPP
#define BOOST_BEAST2_WRITE_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/system/error_code.hpp>

namespace boost {
namespace beast2 {

/** Write HTTP data to a stream
*/
template<
    class AsyncWriteStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(
                typename AsyncWriteStream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_write_some(
    AsyncWriteStream& dest,
    http_proto::serializer& sr,
    CompletionToken&& token
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(
            typename AsyncWriteStream::executor_type));

/** Write HTTP data to a stream
*/
template<
    class AsyncWriteStream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(
                typename AsyncWriteStream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_write(
    AsyncWriteStream& dest,
    http_proto::serializer& sr,
    CompletionToken&& token
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(
            typename AsyncWriteStream::executor_type));

#if 0
/**
*/
template<
    class AsyncWriteStream,
    class AsyncReadStream,
    class CompletionCondition,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(
        void(system::error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(
                typename AsyncWriteStream::executor_type)>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    void (system::error_code, std::size_t))
async_relay_some(
    AsyncWriteStream& dest,
    AsyncReadStream& src,
    CompletionCondition const& cond,
    http_proto::serializer& sr,
    CompletionToken&& token
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(
            typename AsyncWriteStream::executor_type));
#endif

} // beast2
} // boost

#include <boost/beast2/impl/write.hpp>

#endif
