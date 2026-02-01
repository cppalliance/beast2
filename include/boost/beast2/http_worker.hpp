//
// Copyright (c) 2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_HTTP_WORKER_HPP
#define BOOST_BEAST2_HTTP_WORKER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/capy/io/any_read_stream.hpp>
#include <boost/capy/task.hpp>
#include <boost/http/config.hpp>
#include <boost/http/request_parser.hpp>
#include <boost/http/serializer.hpp>
#include <boost/http/server/flat_router.hpp>
#include <boost/http/server/router.hpp>

namespace boost {
namespace beast2 {

/** Reusable HTTP request/response processing logic.

    This class provides the core HTTP processing loop: reading
    requests, dispatching them through a router, and sending
    responses. It is designed as a mix-in base class for use
    with @ref corosio::tcp_server.

    @par Usage with tcp_server

    To use this class, derive a custom worker from both
    @ref corosio::tcp_server::worker_base and `http_worker`.
    The derived class must:

    @li Construct `http_worker` with a router and configurations
    @li Initialize the @ref stream member before calling
        @ref do_http_session
    @li Wire the parser and serializer to the socket by setting
        `rp.req_body` and `rp.res_body`

    @par Example
    @code
    struct my_worker
        : tcp_server::worker_base
        , http_worker
    {
        corosio::tcp_socket sock;

        my_worker(
            corosio::io_context& ctx,
            http::flat_router const& router,
            http::shared_parser_config parser_cfg,
            http::shared_serializer_config serializer_cfg)
            : http_worker(router, parser_cfg, serializer_cfg)
            , sock(ctx)
        {
            sock.open();
            rp.req_body = capy::any_buffer_source(parser.source_for(sock));
            rp.res_body = capy::any_buffer_sink(serializer.sink_for(sock));
            stream = capy::any_read_stream(&sock);
        }

        corosio::tcp_socket& socket() override { return sock; }

        void run(launcher launch) override
        {
            launch(sock.get_executor(), do_http_session());
        }
    };
    @endcode

    @par Thread Safety
    Distinct objects: Safe.
    Shared objects: Unsafe.

    @see corosio::tcp_server, http_server
*/
class BOOST_BEAST2_DECL http_worker
{
public:
    http::flat_router fr;
    http::route_params rp;
    capy::any_read_stream stream;
    http::request_parser parser;
    http::serializer serializer;

    /** Construct an HTTP worker.

        @param fr_ The router for dispatching requests to handlers.
        @param parser_cfg Shared configuration for the request parser.
        @param serializer_cfg Shared configuration for the response
            serializer.
    */
    http_worker(
        http::flat_router fr_,
        http::shared_parser_config parser_cfg,
        http::shared_serializer_config serializer_cfg);

    /** Handle an HTTP session.

        This coroutine reads HTTP requests, dispatches them through
        the router, and sends responses until the connection is
        closed or an error occurs. The stream data member must be
        initialized before calling this function.

        @return An awaitable that completes when the session ends.
    */
    capy::task<void>
    do_http_session();
};

} // beast2
} // boost

#endif
