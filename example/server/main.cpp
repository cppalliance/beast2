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
#include <boost/capy/ex/thread_pool.hpp>
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

#if 0
class json_sink : public http::sink
{
public:
    json_sink(json_sink&&) = default;

    json_sink(
        json::storage_ptr sp = {})
        : pr_(new json::parser(std::move(sp)))
    {
    }

    auto release() -> json::value
    {
        return pr_->release();
    }

private:
    results
    on_write(
        capy::const_buffer b,
        bool more) override
    {
        results rv;
        if(more)
        {
            rv.bytes = pr_->write_some(
                static_cast<char const*>(
                    b.data()), b.size(), rv.ec);
        }
        else
        {
            rv.bytes = pr_->write(
                static_cast<char const*>(b.data()),
                b.size(), rv.ec);
        }
        if(! rv.ec.failed())
            return rv;
        return rv;
    }

    std::unique_ptr<json::parser> pr_;
};

struct do_json_rpc
{
    auto operator()(
        http::route_params& rp) const ->
            http::route_result
    {
        if(! rp.is_method(http::method::post))
            return http::route::next;
        return rp.read_body(
            json_sink(),
            [this](
                json::value jv) ->
                    http::route_result
            {
                return dispatch(std::move(jv));
            });
    }

    // process the JSON-RPC request
    auto dispatch(
        json::value jv) const ->
            http::route_result
    {
        (void)jv;
        return http::route::next;
    }

};
#endif

#if 0

auto
do_bcrypt(
    http::route_params& rp) ->
        capy::task<http::route_result>
{
    std::string password = "boost";
    //auto rv = co_await capy::bcrypt::async_hash(password);
    co_await g_tp.get_executor().submit(
        []()
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        });
    co_return http::route::next;
}


#endif

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

        corosio::io_context ioc;

        install_services();

        auto addr = urls::parse_ipv4_address(argv[1]);
        if(addr.has_error())
        {
            std::cerr << "Invalid address: " << argv[1] << "\n";
            return EXIT_FAILURE;
        }
        auto port = static_cast<std::uint16_t>(std::atoi(argv[2]));
        corosio::endpoint ep(addr.value(), port);

        corosio_router rr;

        rr.use( "/", http::serve_static( argv[3] ) );
#if 0
        rr.use(
            [](http::route_params& rp) -> capy::task<http::route_result>
            {
                co_return co_await rp.send( "Hello, coworld!" );
            });
#endif
        auto parser_cfg = http::make_parser_config(http::parser_config(true));
        auto serializer_cfg = http::make_serializer_config(http::serializer_config());

        http_server hsrv(
            ioc,
            std::atoi(argv[4]),
            std::move(rr),
            std::move(parser_cfg),
            std::move(serializer_cfg));
        auto ec = hsrv.bind(ep);
        if(ec)
        {
            std::cerr << "Bind failed: " << ec.message() << "\n";
            return EXIT_FAILURE;
        }

        hsrv.start();
        ioc.run();

#if 0
        auto& srv = install_plain_http_server(
            app,
            argv[1],
            (unsigned short)std::atoi(argv[2]),
            std::atoi(argv[4]));

        http::cors_options opts;
        opts.allowedHeaders = "Content-Type";

        srv.wwwroot.use(
            []( http::route_params& rp ) ->
                capy::task<http::route_result>
            {
                co_return {};
            });
        srv.wwwroot.use(
            "/rpc",
            http::cors(opts),
            do_json_rpc()
        );

        srv.wwwroot.use( "/bcrypt", do_bcrypt );

        srv.wwwroot.use("/", serve_static( argv[3] ));

        app.start();
        srv.run();
#endif
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
