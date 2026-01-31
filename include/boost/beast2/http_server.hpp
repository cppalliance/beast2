//
// Copyright (c) 2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_HTTP_SERVER_HPP
#define BOOST_BEAST2_HTTP_SERVER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/corosio/tcp_server.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/http/config.hpp>
#include <cstddef>

namespace boost {
namespace http { class flat_router; }
namespace beast2 {

/** An HTTP server for handling requests with coroutine-based I/O.

    This class provides a complete HTTP server implementation that
    accepts connections, parses HTTP requests, and dispatches them
    through a router. Each connection is handled by a worker that
    processes requests using coroutines.

    @par Thread Safety
    Distinct objects: Safe.
    Shared objects: Unsafe.

    @par Example
    @code
    corosio::io_context ctx;
    http::flat_router router;
    router.add( http::verb::get, "/", my_handler );

    http_server srv(
        ctx,
        4,  // workers
        std::move( router ),
        http::shared_parser_config::make(),
        http::shared_serializer_config::make() );

    srv.listen( "0.0.0.0", 8080 );
    ctx.run();
    @endcode
*/
class BOOST_BEAST2_DECL
    http_server : public corosio::tcp_server
{
    struct impl;
    impl* impl_;

    class http_worker;
    struct worker;

public:
    /// Destroy the server.
    ~http_server();

    /** Construct an HTTP server.

        @param ctx The I/O context for asynchronous operations.
        @param num_workers Number of worker objects for handling
            connections concurrently.
        @param router The router for dispatching requests to handlers.
        @param parser_cfg Shared configuration for request parsers.
        @param serializer_cfg Shared configuration for response
            serializers.
    */
    http_server(
        corosio::io_context& ctx,
        std::size_t num_workers,
        http::flat_router router,
        http::shared_parser_config parser_cfg,
        http::shared_serializer_config serializer_cfg);
};

} // beast2
} // boost

#endif
