//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "certificate.hpp"
#include "worker_ssl.hpp"
#include "serve_detached.hpp"
#include <boost/beast2/server/router_asio.hpp>
#include <boost/beast2/server/server_asio.hpp>
#include <boost/beast2/server/serve_static.hpp>
#include <boost/beast2/server/workers.hpp>
#include <boost/beast2/error.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/rts/brotli/decode.hpp>
#include <boost/rts/brotli/encode.hpp>
#include <boost/rts/zlib/deflate.hpp>
#include <boost/rts/zlib/inflate.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <functional>
#include <iostream>

namespace boost {
namespace beast2 {

int server_main( int argc, char* argv[] )
{
    try
    {
        // Check command line arguments.
        if (argc != 5)
        {
            std::cerr << "Usage: " << argv[0] << " <address> <port> <doc_root> <num_workers>\n";
            std::cerr << "  For IPv4, try:\n";
            std::cerr << "    " << argv[0] << " 0.0.0.0 80 . 100\n";
            std::cerr << "  For IPv6, try:\n";
            std::cerr << "    " << argv[0] << " 0::0 80 . 100\n";
            return EXIT_FAILURE;
        }

        auto const addr = asio::ip::make_address(argv[1]);
        unsigned short const port = static_cast<unsigned short>(std::atoi(argv[2]));
        (void)port;
        std::string const doc_root = argv[3];
        std::size_t num_workers = std::atoi(argv[4]);
        int num_threads = 1;
        bool reuse_addr = true; // VFALCO this could come from the command line

        using executor_type = server_asio::executor_type;

        server_asio srv(num_threads);

        #ifdef BOOST_RTS_HAS_BROTLI
        rts::brotli::install_decode_service(srv.services());
        rts::brotli::install_encode_service(srv.services());
        #endif
        #ifdef BOOST_RTS_HAS_ZLIB
        rts::zlib::install_deflate_service(srv.services());
        rts::zlib::install_inflate_service(srv.services());
        #endif

        // The asio::ssl::context holds the certificates and
        // various states needed for the OpenSSL stream implementation.
        asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12);

        // This loads a self-signed certificate. You MUST replace this with your
        // own certificate signed by a well-known Trusted Certificate Authority.
        // For testing, you can import the file examples/beast-test-CA.crt into
        // your browser or operating system's trusted certificate store in order
        // for the browser to not give constant warnings when connecting to
        // the server via HTTPS.
        //
        load_server_certificate(ssl_ctx);

        // VFALCO These ugly incantations are needed for http_proto and will hopefully go away soon.
        {
            http_proto::request_parser::config cfg;
            http_proto::install_parser_service(srv.services(), cfg);
        }
        {
            http_proto::serializer::config cfg;
            http_proto::install_serializer_service(srv.services(), cfg);
        }

        using workers_type = workers< worker_ssl<executor_type> >;
        using stream_type = worker_ssl< executor_type>::stream_type;

        router_asio< stream_type > app;

        app.use("/detach", serve_detached());

        // root site
        app.use("/", serve_static( doc_root ));

        // unhandled errors
        app.err(
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

        // Create all our workers
        auto& vp = emplace_part<workers_type>(
            srv, srv.get_executor(), 1, num_workers,
            srv.get_executor(), ssl_ctx, app);

        // SSL port
        vp.emplace(
            acceptor_config{ true, false },
            workers_type::acceptor_type(
                srv.get_executor(),
                asio::ip::tcp::endpoint(addr, 443),
                reuse_addr));

        // plain port
        vp.emplace(
            acceptor_config{ false, false },
            workers_type::acceptor_type(
                srv.get_executor(),
                asio::ip::tcp::endpoint(addr, 80),
                reuse_addr));

        srv.run();
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
/*

workers
    provide acceptors
    has an executor
    needs a socket to accept into

worker_plain
worker_ssl
worker_flex
    has an executor
    provides stream()

http_responder
    inject
        server&
        router&
        Stream&
        close_fn

    do_session: called when a new connection is accepted
    calls external do_close() to notify end of session
    calls derived class do_fail() to log an error
    uses the executor of the stream

*/