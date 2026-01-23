//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTE_HANDLER_COROSIO_HPP
#define BOOST_BEAST2_SERVER_ROUTE_HANDLER_COROSIO_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http/server/route_handler.hpp>
#include <boost/http/field.hpp>
#include <boost/http/string_body.hpp>
#include <boost/corosio/socket.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/buffers/copy.hpp>
#include <boost/capy/write.hpp>

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

    corosio::socket& stream;

    explicit
    corosio_route_params(
        corosio::socket& s)
        : stream(s)
    {
    }

    http::route_task
    send(std::string_view body) override
    {
        // Auto-detect Content-Type if not already set
        if(! res.exists(http::field::content_type))
        {
            if(! body.empty() && body[0] == '<')
                res.set(http::field::content_type,
                    "text/html; charset=utf-8");
            else
                res.set(http::field::content_type,
                    "text/plain; charset=utf-8");
        }

        // Set Content-Length if not already set
        if(! res.exists(http::field::content_length))
            res.set_payload_size(body.size());

        // Start the serializer with the response and body
        serializer.start(res,
            http::string_body(std::string(body)));

        // Write the response to the socket
        while(! serializer.is_done())
        {
            auto rv = serializer.prepare();
            if(! rv)
                co_return rv.error();

            auto bufs = *rv;

            // Write all buffers
            for(auto const& buf : bufs)
            {
                auto [ec, n] = co_await stream.write_some(
                    capy::const_buffer(buf.data(), buf.size()));
                if(ec)
                    co_return ec;
            }

            // Calculate total size written
            std::size_t written = 0;
            for(auto const& buf : bufs)
                written += buf.size();

            serializer.consume(written);
        }

        co_return {};
    }

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

            if(capy::buffer_size(*cbs) == 0)
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
    http::route_task
    write_impl(capy::const_buffer_param buffers) override
    {
        // Initialize streaming on first call
        if(!srs_.is_open())
            srs_ = serializer.start_stream(res);

        // Loop until all input buffers are consumed
        while(true)
        {
            auto bufs = buffers.data();
            if(bufs.empty())
                break;

            // Copy data to serializer stream
            std::size_t bytes = capy::copy(srs_.prepare(), bufs);
            srs_.commit(bytes);
            buffers.consume(bytes);

            // Write serializer output to socket
            auto cbs = serializer.prepare();
            if(cbs.has_error())
            {
                if(cbs.error() != http::error::need_data)
                    co_return cbs.error();
                continue;
            }

            auto [ec, n] = co_await capy::write(stream, *cbs);
            if(ec)
                co_return ec;
            serializer.consume(capy::buffer_size(*cbs));
        }

        co_return {};
    }
};

} // beast2
} // boost

#endif
