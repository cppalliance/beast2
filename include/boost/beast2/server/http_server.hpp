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
#include <boost/beast2/server/router_asio.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace boost {
namespace beast2 {

class application;

template<class AsyncStream>
class http_server
{
public:
    ~http_server() = default;

    http_server() = default;

    router_asio<AsyncStream&> wwwroot;

    /** Run the server

        This function attaches the current thread to I/O context
        so that it may be used for executing submitted function
        objects. Blocks the calling thread until the part is stopped
        and has no outstanding work.
    */
    virtual void attach() = 0;
};

//------------------------------------------------

BOOST_BEAST2_DECL
auto
install_plain_http_server(
    application& app,
    char const* addr,
    unsigned short port,
    std::size_t num_workers) ->
        http_server<asio::basic_stream_socket<
            asio::ip::tcp,
            asio::io_context::executor_type>>&;

} // beast2
} // boost

#endif
