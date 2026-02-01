//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "certificate.hpp"
#include "serve_log_admin.hpp"
#include <boost/http/field.hpp>
#include <boost/http/server/router.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/http_server.hpp>
#include <boost/beast2/https_server.hpp>
#include <boost/capy/buffers/string_dynamic_buffer.hpp>
#include <boost/capy/ex/thread_pool.hpp>
#include <boost/capy/io/push_to.hpp>
#include <boost/capy/read.hpp>
#include <boost/corosio/signal_set.hpp>
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
#include <csignal>
#include <iostream>
#include <string>

namespace boost {
namespace beast2 {

/** Route handler that redirects to a different URL.

    Returns a handler that sends an HTTP redirect response with
    the specified status code and Location header. The response
    includes an HTML body with a clickable link for browsers
    that don't auto-redirect.

    @par Example
    @code
    rr.use("/old-page", redirect("/new-page"));

    rr.use("/moved", redirect("/new-location", http::status::moved_permanently));
    @endcode

    @param url The URL to redirect to. Can be absolute or relative.
    @param code The HTTP status code. Defaults to 302 (Found).

    @return A route handler.
*/
struct redirect
{
    std::string url_;
    http::status code_;

    redirect(
        std::string_view url,
        http::status code = http::status::found)
        : url_(url)
        , code_(code)
    {
    }

    http::route_task operator()(http::route_params& rp) const
    {
        rp.status(code_);
        rp.res.set(http::field::location, url_);
        rp.res.set(http::field::content_type, "text/html; charset=utf-8");

        std::string body;
        body.reserve(128 + url_.size() * 2);
        body.append("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
            "<title>Redirecting</title></head><body>"
            "<p>Redirecting to <a href=\"");
        body.append(url_);
        body.append("\">");
        body.append(url_);
        body.append("</a></p></body></html>");

        auto [ec] = co_await rp.send(body);
        if(ec)
            co_return http::route_error(ec);
        co_return http::route_done;
    }
};

struct https_redirect
{
    http::route_task operator()(http::route_params& rp) const
    {
        std::string url = "https://";
        url += rp.req.at(http::field::host);
        url += rp.url.encoded_path();
        if(rp.url.has_query())
        {
            url += '?';
            url += rp.url.encoded_query();
        }
        
        //rp.status(http::status::moved_permanently);
        rp.status(http::status::found);
        rp.res.set(http::field::location, url);
        auto [ec] = co_await rp.send("pupu");
        if(ec)
            co_return http::route_error(ec);
        co_return http::route_done;
    }
};

void install_services()
{
#ifdef BOOST_HTTP_HAS_BROTLI
    http::brotli::install_brotli_service();
#endif

#ifdef BOOST_HTTP_HAS_ZLIB
    http::zlib::install_zlib_service();
#endif
}

class application : public http::router
{
    struct impl;
    impl* impl_;

public:
    void listen(unsigned short)
    {
        http::flat_router fr(std::move(*this));
    }
};

/*
void test()
{
    beast2::app app;
    app.use( "/", serve_static( "C:\\Users\\Vinnie\\www-root" ) );
    app.listen(80, 443);
}
*/

int server_main( int argc, char* argv[] )
{
    // Check command line arguments.
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <num_workers> <doc_root>\n";
        return EXIT_FAILURE;
    }

    corosio::io_context ioc;

    install_services();

    corosio::ipv4_address addr;
    corosio::endpoint ep(addr, 0);

    http::router rr1;
    rr1.use( https_redirect() );
#if 0
    rr1.use( "/api",
        [&]( auto& rp ) -> http::route_task
        {
            if(rp.req.method() != http::method::post)
                co_return http::route_next;
            http::json_sink js;
            auto [ec, n] = co_await capy::push_to(rp.req_body, js);
            if(ec)
                co_return http::route_error(ec);
            json::value jv = js.release();
            co_return http::route_next;
        });
    // app.get('/', (req, res) => res.send('Hello'));
    rr1.use( "/", []( auto& rp ) -> http::route_task
        {
            auto [ec] = co_await rp.send("Hello");
            (void)ec;
            co_return http::route_done;
        });
#endif
    http_server hs1(ioc, 40, http::flat_router(std::move(rr1)),
        http::make_parser_config(http::parser_config(true)),
        http::make_serializer_config(http::serializer_config()));
    auto ec = hs1.bind(corosio::endpoint(ep, 80));
    if(ec)
    {
        std::cerr << "Bind failed: " << ec.message() << "\n";
        return EXIT_FAILURE;
    }
    hs1.start();

#ifdef BOOST_COROSIO_HAS_OPENSSL
    corosio::tls_context tls;
    load_server_certificate(tls);
    http::router rr2;
    rr2.use( http::cors() );
    rr2.use( "/", http::serve_static( argv[2] ) );
    https_server hs2(ioc, std::atoi(argv[1]), tls,
        http::flat_router(std::move(rr2)),
        http::make_parser_config(http::parser_config(true)),
        http::make_serializer_config(http::serializer_config()));
    ec = hs2.bind(corosio::endpoint(ep, 443));
    if(ec)
    {
        std::cerr << "Bind failed: " << ec.message() << "\n";
        return EXIT_FAILURE;
    }
    hs2.start();
#endif

    corosio::signal_set sigs(ioc);
    sigs.add(SIGINT);
    capy::run_async(ioc.get_executor())(
        [&]() -> capy::task<void>
        {
            auto [ec, sig] = co_await sigs.wait();
            if(ec)
                throw std::system_error(ec);
            hs1.stop();
        #ifdef BOOST_COROSIO_HAS_OPENSSL
            hs2.stop();
        #endif
        }());

    ioc.run();

    hs1.join();
#ifdef BOOST_COROSIO_HAS_OPENSSL
    hs2.join();
#else
# error die!
#endif

    return EXIT_SUCCESS;
}

} // beast2
} // boost

int main(int argc, char* argv[])
{
    return boost::beast2::server_main( argc, argv );
}
