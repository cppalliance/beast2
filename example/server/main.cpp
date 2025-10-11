//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "asio_server.hpp"
#include "fixed_array.hpp"
#include "listening_port.hpp"
#include "worker.hpp"
#include "worker_ssl.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/serializer.hpp>
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
        std::string const doc_root = argv[3];
        std::size_t num_workers = std::atoi(argv[4]);
        bool reuse_addr = true; // VFALCO this could come from the command line

        using executor_type = asio_server::executor_type;

        asio_server srv;

        {
            http_proto::request_parser::config cfg;
            http_proto::install_parser_service(srv.services(), cfg);
        }
        {
            http_proto::serializer::config cfg;
            http_proto::install_serializer_service(srv.services(), cfg);
        }

        {
            // Create the workers and construct the port
            fixed_array<worker<executor_type>> wv(num_workers);
            while(! wv.is_full())
                wv.append(srv, doc_root);
            new_part<boost::http_io::listening_port<executor_type>>(
                srv,
                std::move(wv),
                asio::ip::tcp::endpoint(addr, port),
                srv.get_executor(),
                reuse_addr);
        }

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
