//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_COROSIO_ROUTER_HPP
#define BOOST_BEAST2_SERVER_COROSIO_ROUTER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http/server/router.hpp>
#include <boost/http/field.hpp>
#include <boost/http/string_body.hpp>
#include <boost/corosio/socket.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/buffers/buffer_copy.hpp>
#include <boost/capy/write.hpp>
#include <span>

namespace boost {
namespace beast2 {

/** Route parameters object for Corosio HTTP route handlers
*/
class corosio_route_params
    : public http::route_params
{
    http::serializer::stream srs_;

public:
    using stream_type = corosio::socket;

    corosio::io_stream& stream;

    explicit
    corosio_route_params(
        corosio::io_stream& s)
        : stream(s)
    {}

    http::route_task
    end() override
    {
        // Close the serializer stream if active
        if(srs_.is_open())
            srs_.close();

        // Drain all remaining serializer output
        while(!serializer.is_done())
        {
            auto cbs = serializer.prepare();
            if(cbs.has_error())
            {
                if(cbs.error() == http::error::need_data)
                    continue;
                co_return cbs.error();
            }

            if(capy::buffer_empty(*cbs))
            {
                serializer.consume(0);
                continue;
            }

            auto [ec, n] = co_await capy::write(stream, *cbs);
            if(ec)
                co_return ec;
            serializer.consume(capy::buffer_size(*cbs));
        }

        co_return {};
    }

protected:
    capy::task<capy::io_result<std::size_t>>
    write_impl(
        std::span<capy::const_buffer> buffers) override
    {
        // Initialize streaming on first call
        if(!srs_.is_open())
            srs_ = serializer.start_stream(res);

        // Copy input buffers to serializer stream
        auto bytes_written = capy::buffer_copy(srs_.prepare(), buffers);
        srs_.commit(bytes_written);

        // Flush serializer output to socket
        while(true)
        {
            auto cbs = serializer.prepare();
            if(cbs.has_error())
            {
                if(cbs.error() != http::error::need_data)
                    co_return {cbs.error()};
                break;
            }

            auto [ec, n] = co_await capy::write(stream, *cbs);
            if(ec)
                co_return {ec};
            serializer.consume(capy::buffer_size(*cbs));
        }

        co_return {{}, bytes_written};
    }
};

/** The Corosio-aware router type
*/
using corosio_router = http::basic_router<corosio_route_params>;

} // beast2
} // boost

#endif
