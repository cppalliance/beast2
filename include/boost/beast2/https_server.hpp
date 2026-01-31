//
// Copyright (c) 2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_HTTPS_SERVER_HPP
#define BOOST_BEAST2_HTTPS_SERVER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/corosio/tcp_server.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/corosio/tls/context.hpp>
#include <boost/http/config.hpp>
#include <cstddef>

namespace boost {
namespace http { class flat_router; }
namespace beast2 {

/** An HTTPS server for handling requests with coroutine-based I/O.

    This class provides a complete HTTPS server implementation that
    accepts connections, performs TLS handshake, parses HTTP requests,
    and dispatches them through a router. Each connection is handled
    by a worker that processes requests using coroutines over an
    encrypted TLS stream.

    @par Thread Safety
    Distinct objects: Safe.
    Shared objects: Unsafe.

    @par Example
    @code
    corosio::io_context ctx;

    // Configure TLS context for server
    corosio::tls::context tls_ctx;
    tls_ctx.use_certificate_chain_file("server.crt", tls::file_format::pem);
    tls_ctx.use_private_key_file("server.key", tls::file_format::pem);

    http::flat_router router;
    router.add( http::verb::get, "/", my_handler );

    https_server srv(
        ctx,
        4,  // workers
        std::move( tls_ctx ),
        std::move( router ),
        http::shared_parser_config::make(),
        http::shared_serializer_config::make() );

    srv.listen( "0.0.0.0", 8443 );
    ctx.run();
    @endcode
*/
class BOOST_BEAST2_DECL
    https_server : public corosio::tcp_server
{
    struct impl;
    impl* impl_;

    struct worker;

public:
    /// Destroy the server.
    ~https_server();

    /** Construct an HTTPS server.

        @param ctx The I/O context for asynchronous operations.
        @param num_workers Number of worker objects for handling
            connections concurrently.
        @param tls_ctx The TLS context containing certificate and
            key configuration. The context is copied and shared
            among all workers.
        @param router The router for dispatching requests to handlers.
        @param parser_cfg Shared configuration for request parsers.
        @param serializer_cfg Shared configuration for response
            serializers.
    */
    https_server(
        corosio::io_context& ctx,
        std::size_t num_workers,
        corosio::tls::context tls_ctx,
        http::flat_router router,
        http::shared_parser_config parser_cfg,
        http::shared_serializer_config serializer_cfg);
};

} // beast2
} // boost

#endif
