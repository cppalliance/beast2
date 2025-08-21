//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef JSONRPC_ANY_STREAM_HPP
#define JSONRPC_ANY_STREAM_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/buffers/const_buffer_span.hpp>
#include <boost/buffers/mutable_buffer_span.hpp>
#include <boost/url/url_view.hpp>

namespace jsonrpc {

/// Interface for a stream that can be used with @ref client.
class any_stream
{
public:
    using executor_type = boost::asio::any_io_executor;

    virtual boost::asio::any_io_executor
    get_executor() = 0;

    virtual void
    async_connect(
        boost::urls::url_view,
        boost::asio::any_completion_handler<void(boost::system::error_code)>) = 0;

    virtual void
    async_write_some(
        const boost::buffers::const_buffer_subspan&,
        boost::asio::any_completion_handler<void(boost::system::error_code, std::size_t)>) = 0;

    virtual void
    async_read_some(
        const boost::buffers::mutable_buffer_subspan&,
        boost::asio::any_completion_handler<void(boost::system::error_code, std::size_t)>) = 0;

    virtual void
    async_shutdown(
        boost::asio::any_completion_handler<void(boost::system::error_code)>) = 0;

    virtual ~any_stream() = default;
};

} // jsonrpc

#endif
