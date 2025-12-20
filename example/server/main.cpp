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
#include <boost/json/parser.hpp>
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
    http::install_parser_service(app,
        http::request_parser::config());
    http::install_serializer_service(app,
        http::serializer::config());
}

/*

struct json_rpc;

// Handle POST by parsing JSON-RPC,
// storing `json_rpc` in `rp.request_data`
// This validates the JSON and can respond with an error
//
app.use("/rpc", json_rpc_post())

// Process the JSON-RPC command
app.post(
    "/rpc",
    json_post(),
    do_json_rpc()
    );

*/

class json_sink : public http::sink
{
public:
    explicit
    json_sink(
        json::value& jv) : jv_(jv)
    {
    }
private:
    results
    on_write(
        buffers::const_buffer b,
        bool more) override
    {
        results rv;
        if(more)
        {
            rv.bytes = pr_.write_some(
                static_cast<char const*>(
                    b.data()), b.size(), rv.ec);
        }
        else
        {
            rv.bytes = pr_.write(
                static_cast<char const*>(b.data()),
                b.size(), rv.ec);
        }
        if(! rv.ec.failed())
        {
            jv_ = pr_.release();
            return rv;
        }
        return rv;
    }

    json::value& jv_;
    json::parser pr_;
};

struct post_json_rpc
{
    auto operator()(
        http::route_params& rp) const ->
            http::route_result
    {
        if(! rp.is_method(http::method::post))
            return http::route::next;
        BOOST_ASSERT(rp.parser.is_complete());
        auto& jv = rp.route_data.emplace<json::value>();
        rp.parser.set_body<json_sink>(jv);
        system::error_code ec;
        rp.parser.parse(ec);
        if(ec.failed())
            return ec;
        return http::route::next;
    }
};

struct do_json_rpc
{
    auto operator()(
        http::route_params&) const ->
            http::route_result
    {
        return http::route::next;
    }
};

auto
do_request(
    http::route_params& rp) ->
        capy::task<http::route_result>
{
    co_return http::route::next;
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

        http::cors_options opts;
        opts.allowedHeaders = "Content-Type";

        srv.wwwroot.use( http::co_route( do_request ) );

        srv.wwwroot.use("/rpc",
            http::cors(opts),
            post_json_rpc(),
            []( http::route_params& rp) ->
                http::route_result
            {
                if(rp.parser.is_complete())
                {
                    auto s = rp.parser.body();
                    return http::route::next;
                }
                return http::route::next;
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
