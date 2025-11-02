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
#include <boost/beast2/application.hpp>
#include <boost/beast2/server/router_asio.hpp>
#include <boost/beast2/server/workers.hpp>
#include <boost/asio/ip/tcp.hpp>
//#include "worker_ssl.hpp"

namespace boost {
namespace beast2 {

/*
enum server_type
{
    single_threaded_plain
    ,multi_threaded_plain
//#ifdef BOOST_BEAST2_USE_SSL
    ,single_threaded_ssl
    ,multi_threaded_ssl
    ,single_threaded_flex
    ,multi_threaded_flex
//#endif
};
*/
/*

// single threaded plain
asio::basic_stream_socket<
    asio::ip::tcp,
    asio::io_context::executor_type>

// single threaded SSL
asio::ssl::stream<
    asio::basic_stream_socket<
        asio::ip::tcp,
        asio::io_context::executor_type> >

// single threaded flex
ssl_stream<
    asio::basic_stream_socket<
        asio::ip::tcp,
        asio::io_context::executor_type> >

// multi threaded plain
asio::basic_stream_socket<
    asio::ip::tcp,
    asio::strand< asio::io_context::executor_type> >

// multi threaded SSL
asio::ssl::stream<
    asio::basic_stream_socket<
        asio::ip::tcp,
        asio::strand< asio::io_context::executor_type> > >

// multi threaded flex
ssl_stream<
    asio::basic_stream_socket<
        asio::ip::tcp,
        asio::strand< asio::io_context::executor_type> > >


*/
template<class Stream>
struct http_server
{
    using stream_type = Stream;
    using router_type = router_asio<stream_type>;
    using executor_type = decltype(
        std::declval<Stream&>().get_executor());

    virtual std::size_t concurrency() const noexcept = 0;
};

auto
make_single_threaded_flex_http_server(
    application& app,
    std::size_t num_workers) ->
        http_server<
            asio::basic_stream_socket<
                asio::ip::tcp,
                asio::io_context::executor_type>>&;


} // beast2
} // boost

#endif
