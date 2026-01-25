//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "serve_log_admin.hpp"
#include <boost/beast2/corosio_router.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/http_server.hpp>
#include <boost/capy/buffers/string_dynamic_buffer.hpp>
#include <boost/capy/ex/thread_pool.hpp>
#include <boost/capy/io/push_to.hpp>
#include <boost/capy/read.hpp>
#include <boost/http/json/json_sink.hpp>
#include <boost/http/server/flat_router.hpp>
#include <boost/http/server/serve_static.hpp>
#include <boost/http/request_parser.hpp>
#include <boost/http/serializer.hpp>
#include <boost/http/server/cors.hpp>
#include <boost/http/bcrypt.hpp>
#include <boost/http/brotli/decode.hpp>
#include <boost/http/brotli/encode.hpp>
#include <boost/http/zlib/deflate.hpp>
#include <boost/http/zlib/inflate.hpp>
#include <boost/json/parser.hpp>
#include <boost/url/ipv4_address.hpp>
#include <iostream>

namespace boost {
namespace beast2 {

capy::thread_pool g_tp;

void install_services()
{
#ifdef BOOST_HTTP_HAS_BROTLI
    http::brotli::install_brotli_service();
#endif

#ifdef BOOST_HTTP_HAS_ZLIB
    http::zlib::install_zlib_service();
#endif
}

int server_main( int argc, char* argv[] )
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <num_workers> <doc_root>\n";
        return EXIT_FAILURE;
    }

    corosio::io_context ioc;

    install_services();

    urls::ipv4_address addr;
    auto port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    corosio::endpoint ep(addr, port);

    corosio_router rr;
    rr.use( http::cors() );
    rr.use(
        [&]( auto& rp ) -> http::route_task
        {
            if(rp.req.method() != http::method::post)
                co_return http::route::next;
            http::json_sink js;
            auto [ec, n] = co_await capy::push_to(rp.req_bufs, js);
            if(ec.failed())
                co_return ec;
            (void)n;
            json::value jv = js.release();
            co_return {};
        });
   rr.use( "/", http::serve_static( argv[3] ) );
  
    http_server hsrv(
        ioc,
        std::atoi(argv[2]),
        std::move(rr),
        http::make_parser_config(http::parser_config(true)),
        http::make_serializer_config(http::serializer_config()));
    auto ec = hsrv.bind(ep);
    if(ec)
    {
        std::cerr << "Bind failed: " << ec.message() << "\n";
        return EXIT_FAILURE;
    }

    hsrv.start();
    ioc.run();
    return EXIT_SUCCESS;
}

} // beast2
} // boost

int main(int argc, char* argv[])
{
    return boost::beast2::server_main( argc, argv );
}
