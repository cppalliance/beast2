//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "asio_server.hpp"
#include "certificate.hpp"
#include "listening_port.hpp"
#include "worker.hpp"
#include "worker_ssl.hpp"
#include <boost/http_io/server/ports.hpp>
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
namespace http_io {

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
        bool reuse_addr = true; // VFALCO this could come from the command line

        using executor_type = asio_server::executor_type;

        asio_server srv;

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

        // Create the routes
        // VFALCO this is a hacky WIP
        router_type rr0;
        rr0.get<https_redirect_responder>("/*splat");
        router_type rr;
        rr.get<file_responder>("/*splat", doc_root);

        auto& vp = emplace_part<ports<executor_type>>(srv,
            srv.get_executor(),
            1,
            num_workers,
            of_type<worker_ssl<executor_type>>,
            srv,
            srv.get_executor(),
            ssl_ctx,
            rr);
        vp.emplace(asio::ip::tcp::endpoint(addr, 443), reuse_addr);
        //vp.emplace(asio::ip::tcp::endpoint(addr, 5050), reuse_addr);

        //
        // Add the listening ports and workers
        //
#if 0
        {
            // plain (no https) port that does https redirect
            //
            auto& lp = emplace_part<listening_port<executor_type>>(
                srv,
                asio::ip::tcp::endpoint(
                    asio::ip::make_address_v4("127.0.0.1"), 80),
                srv.get_executor(),
                reuse_addr);
            emplace_workers<worker<executor_type>>(
                lp,
                16, // small number of workers
                srv.get_executor(),
                rr0);
        }
        {
            // A secure SSL public listening port
            //
            auto& lp = emplace_part<listening_port<executor_type>>(
                srv,
                asio::ip::tcp::endpoint(addr, 443),
                srv.get_executor(),
                reuse_addr);
            emplace_workers<worker_ssl<executor_type>>(
                lp,
                num_workers,
                srv.get_executor(),
                ssl_ctx,
                rr);
        }
#endif

        srv.run();
    }
    catch( std::exception const& e )
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }    
    return EXIT_SUCCESS;
}

} // http_io
} // boost

int main(int argc, char* argv[])
{
    return boost::http_io::server_main( argc, argv );
}
