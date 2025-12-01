//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "certificate.hpp"
#include "post_work.hpp"
#include "serve_detached.hpp"
#include "serve_log_admin.hpp"
#include <boost/beast2/asio_io_context.hpp>
#include <boost/beast2/server/http_server.hpp>
#include <boost/beast2/server/router.hpp>
#include <boost/beast2/server/serve_static.hpp>
#include <boost/beast2/error.hpp>
#include <boost/capy/application.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/http_proto/server/cors.hpp>
#include <boost/capy/brotli/decode.hpp>
#include <boost/capy/brotli/encode.hpp>
#include <boost/capy/zlib/deflate.hpp>
#include <boost/capy/zlib/inflate.hpp>
#include <iostream>

namespace boost {
namespace beast2 {

void install_services(capy::application& app)
{
#ifdef BOOST_CAPY_HAS_BROTLI
    capy::brotli::install_decode_service(app);
    capy::brotli::install_encode_service(app);
#endif

#ifdef BOOST_CAPY_HAS_ZLIB
    capy::zlib::install_deflate_service(app);
    capy::zlib::install_inflate_service(app);
#endif

    // VFALCO These ugly incantations are needed for http_proto and will hopefully go away soon.
    http_proto::install_parser_service(app,
        http_proto::request_parser::config());
    http_proto::install_serializer_service(app,
        http_proto::serializer::config());
}

int server_main( int argc, char* argv[] )
{
    try
    {
        // Check command line arguments.
        if (argc != 5)
        {
            std::cerr << "Usage: " << argv[0] << " <address> <port> <doc_root> <num_workers>\n";
            return EXIT_FAILURE;
        }

        capy::application app;

        install_services(app);

        auto& srv = install_plain_http_server(
            app,
            argv[1],
            (unsigned short)std::atoi(argv[2]),
            std::atoi(argv[4]));

        //srv.wwwroot.use("/log", serve_log_admin(app));
        //srv.wwwroot.use("/alt", serve_static( argv[3] ));
        //srv.wwwroot.use("/detach", serve_detached());
        //srv.wwwroot.use(post_work());
        srv.wwwroot.use(
            http_proto::cors(),
            []( http_proto::Request& req,
                http_proto::Response& res) ->
                    http_proto::route_result
            {
                return http_proto::route::next;
            });
        srv.wwwroot.use("/", serve_static( argv[3] ));

        app.start();
        srv.attach();
    }
    catch( std::exception const& e )
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }    
    return EXIT_SUCCESS;
}

} // beast2
} // boost

int main(int argc, char* argv[])
{
    return boost::beast2::server_main( argc, argv );
}
