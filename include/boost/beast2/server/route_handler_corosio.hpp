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

namespace boost {
namespace beast2 {

/** Route parameters object for Corosio HTTP route handlers
*/
class corosio_route_params
    : public http::route_params
{
public:
    using stream_type = corosio::socket;

    corosio::socket& stream;

    explicit
    corosio_route_params(
        corosio::socket& s)
        : stream( s )
    {
    }

    http::route_task
    send( std::string_view body ) override
    {
        // Auto-detect Content-Type if not already set
        if( ! res.exists( http::field::content_type ) )
        {
            if( ! body.empty() && body[0] == '<' )
                res.set( http::field::content_type,
                    "text/html; charset=utf-8" );
            else
                res.set( http::field::content_type,
                    "text/plain; charset=utf-8" );
        }

        // Set Content-Length if not already set
        if( ! res.exists( http::field::content_length ) )
            res.set_payload_size( body.size() );

        // Start the serializer with the response and body
        serializer.start( res,
            http::string_body( std::string( body ) ) );

        // Write the response to the socket
        while( ! serializer.is_done() )
        {
            auto rv = serializer.prepare();
            if( ! rv )
                co_return rv.error();

            auto bufs = *rv;

            // Write all buffers
            for( auto const& buf : bufs )
            {
                auto [ec, n] = co_await stream.write_some(
                    capy::const_buffer( buf.data(), buf.size() ) );
                if( ec )
                    co_return ec;
            }

            // Calculate total size written
            std::size_t written = 0;
            for( auto const& buf : bufs )
                written += buf.size();

            serializer.consume( written );
        }

        co_return {};
    }
};

} // beast2
} // boost

#endif
