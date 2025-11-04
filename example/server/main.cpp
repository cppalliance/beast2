//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "certificate.hpp"
#include "serve_detached.hpp"
#include <boost/beast2/application.hpp>
#include <boost/beast2/asio_io_context.hpp>
#include <boost/beast2/server/http_server.hpp>
#include <boost/beast2/server/router.hpp>
#include <boost/beast2/server/serve_static.hpp>
#include <boost/beast2/error.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/rts/brotli/decode.hpp>
#include <boost/rts/brotli/encode.hpp>
#include <boost/rts/zlib/deflate.hpp>
#include <boost/rts/zlib/inflate.hpp>
#include <iostream>

namespace boost {
namespace beast2 {

void install_services(application& app)
{
#ifdef BOOST_RTS_HAS_BROTLI
    rts::brotli::install_decode_service(app.services());
    rts::brotli::install_encode_service(app.services());
#endif

#ifdef BOOST_RTS_HAS_ZLIB
    rts::zlib::install_deflate_service(app.services());
    rts::zlib::install_inflate_service(app.services());
#endif

    // VFALCO These ugly incantations are needed for http_proto and will hopefully go away soon.
    http_proto::install_parser_service(app.services(),
        http_proto::request_parser::config());
    http_proto::install_serializer_service(app.services(),
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

        application app;

        install_services(app);

        auto& srv = install_plain_http_server(
            app,
            argv[1],
            (unsigned short)std::atoi(argv[2]),
            std::atoi(argv[4]));

        {
            router r;
            r.get("/alan",
                [](Request&, Response& res)
                {
                    res.set_body("User: Alan");
                    return error::success;
                });
            r.get("/vinnie",
                [](Request&, Response& res)
                {
                    res.set_body("User: Vinnie");
                    return error::success;
                });
            srv.wwwroot.use("/user", std::move(r));
        }

        srv.wwwroot.use("/", serve_static( argv[3] ));

        // unhandled errors
        srv.wwwroot.err(
            []( Request&, Response& res,
                system::error_code const& ec)
            {
                http_proto::status sc;
                if(ec == system::errc::no_such_file_or_directory)
                    sc = http_proto::status::not_found;
                else
                    sc = http_proto::status::internal_server_error;
                res.status(sc);
                res.set_body(ec.message());
                return error::success;
            });

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
