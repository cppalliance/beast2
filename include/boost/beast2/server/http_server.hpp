//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_HTTP_SERVER_HPP
#define BOOST_BEAST2_SERVER_HTTP_SERVER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/router_corosio.hpp>
#include <boost/capy/application.hpp>
#include <boost/corosio/endpoint.hpp>

namespace boost {
namespace beast2 {

/** An HTTP server using Corosio for I/O.
*/
class http_server
{
public:
    virtual ~http_server() = default;

    http_server() = default;

    /** The router for handling HTTP requests.
    */
    router_corosio wwwroot;

    /** Run the server.

        This function attaches the current thread to the I/O context
        so that it may be used for executing operations. Blocks the
        calling thread until the server is stopped and has no
        outstanding work.
    */
    virtual void run() = 0;

    /** Stop the server.

        Signals the server to stop accepting new connections and
        cancel outstanding operations.
    */
    virtual void stop() = 0;
};

//------------------------------------------------

/** Install a plain (non-TLS) HTTP server into an application.

    @param app The application to install the server into.
    @param addr The address to bind to (e.g. "0.0.0.0").
    @param port The port to listen on.
    @param num_workers The number of worker sockets to preallocate.

    @return A reference to the installed server.
*/
BOOST_BEAST2_DECL
http_server&
install_plain_http_server(
    capy::application& app,
    char const* addr,
    unsigned short port,
    std::size_t num_workers);

} // beast2
} // boost

#endif
