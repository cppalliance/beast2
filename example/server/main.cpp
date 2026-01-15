//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "serve_log_admin.hpp"
#include <boost/beast2/server/http_server.hpp>
#include <boost/beast2/server/router.hpp>
#include <boost/beast2/server/serve_static.hpp>
#include <boost/beast2/error.hpp>
#include <boost/capy/application.hpp>
#include <boost/capy/ex/thread_pool.hpp>
#include <boost/http/request_parser.hpp>
#include <boost/http/serializer.hpp>
#include <boost/http/server/cors.hpp>
#include <boost/capy/bcrypt.hpp>
#include <boost/capy/brotli/decode.hpp>
#include <boost/capy/brotli/encode.hpp>
#include <boost/capy/zlib/deflate.hpp>
#include <boost/capy/zlib/inflate.hpp>
#include <boost/json/parser.hpp>
#include <iostream>

namespace boost {
namespace beast2 {

capy::thread_pool g_tp;

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

    // VFALCO These ugly incantations are needed for http and will hopefully go away soon.
    http::install_parser_service(app,
        http::request_parser::config());
    http::install_serializer_service(app,
        http::serializer::config());
}

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

#if 0
auto
my_coro(
    http::route_params& rp) ->
        capy::task<http::route_result>
{
    (void)rp;
    asio::thread_pool tp(1);
    co_await capy::make_async_op<void>(
        [&tp](auto&& handler)
        {
            asio::post(tp.get_executor(),
                [handler = std::move(handler)]() mutable
                {
                    // Simulate some asynchronous work
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    handler();
                });
        });
    co_return http::route::next;
}

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

        capy::application app;

        install_services(app);

        auto& srv = install_plain_http_server(
            app,
            argv[1],
            (unsigned short)std::atoi(argv[2]),
            std::atoi(argv[4]));

        http::cors_options opts;
        opts.allowedHeaders = "Content-Type";

        srv.wwwroot.use(
            "/rpc",
            http::cors(opts),
            do_json_rpc()
        );
#if 0
        srv.wwwroot.use(
            "/spawn",
            http::co_route(my_coro));
        srv.wwwroot.use(
            "/bcrypt",
            http::co_route(do_bcrypt));
#endif
        srv.wwwroot.use("/", serve_static( argv[3] ));

        app.start();
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
