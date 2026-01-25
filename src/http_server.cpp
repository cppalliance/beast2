//
// Copyright (c) 2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/http_server.hpp>
#include <boost/http/server/flat_router.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/cond.hpp>
#include <boost/capy/ex/strand.hpp>
#include <boost/beast2/corosio_router.hpp>
#include <boost/http/request_parser.hpp>
#include <boost/http/response.hpp>
#include <boost/http/serializer.hpp>
#include <boost/http/string_body.hpp>
#include <boost/http/server/flat_router.hpp>
#include <boost/http/server/basic_router.hpp>
#include <boost/http/error.hpp>
#include <boost/url/parse.hpp>
#include <iostream>

namespace boost {
namespace beast2 {

struct http_server::impl
{
    http::flat_router router;
    http::shared_parser_config parser_cfg;
    http::shared_serializer_config serializer_cfg;

    impl(http::flat_router r)
        : router(std::move(r))
    {
    }
};

// Each worker owns its own socket and parser/serializer state,
// allowing concurrent connection handling without synchronization.
struct http_server::
    worker : tcp_server::worker_base
{
    http_server* srv;
    corosio::io_context& ctx;
    capy::strand<corosio::io_context::executor_type> strand;
    corosio::socket sock;
    corosio_route_params rp;

    worker(
        corosio::io_context& ctx_,
        http_server* srv_)
        : srv(srv_)
        , ctx(ctx_)
        , strand(ctx_.get_executor())
        , sock(ctx_)
        , rp(sock)
    {
        sock.open();
        rp.parser = http::request_parser(srv->impl_->parser_cfg);
        rp.serializer = http::serializer(srv->impl_->serializer_cfg);
    }

    corosio::socket& socket() override
    {
        return sock;
    }

    void run(launcher launch) override
    {
        launch(ctx.get_executor(), do_session());
    }

    capy::task<void>
    do_session()
    {
        struct guard
        {
            corosio_route_params& rp;

            guard(corosio_route_params& rp_)
                : rp(rp_)
            {
            }

            ~guard()
            {
                rp.parser.reset();
                rp.session_data.clear();
                rp.parser.start();
            }
        };

        guard g(rp); // clear things when session ends

        for(;;)
        {
            rp.parser.reset();
            rp.session_data.clear();
            rp.parser.start();

            // Read HTTP request header
            //auto [ec, n] = co_await w.read_header();
            auto [ec] = co_await rp.parser.read_header( sock );
            if (ec.failed())
            {
                std::cerr << "read_header error: " << ec.message() << "\n";
                break;
            }

            //LOG_TRC(sect_)("{} read_header bytes={}", id(), n);

            // Process headers and dispatch
            on_headers();

            auto rv = co_await srv->impl_->router.dispatch(
                rp.req.method(), rp.url, rp);
            (void)rv;
    #if 0
            do_respond(rv); 

            // Write response
            if (!rp.serializer.is_done())
            {
                auto [wec, wn] = co_await write_response();
                if (wec)
                {
                    LOG_TRC(sect_)("{} write_response: {}", id(), wec.message());
                    break;
                }
                LOG_TRC(sect_)("{} write_response bytes={}", id(), wn);
            }

            // Check keep-alive
            if (!rp.res.keep_alive())
                break;
    #endif
        }
    }

    void on_headers()
    {
        // Set up Request and Response objects
        rp.req = rp.parser.get();
        rp.route_data.clear();
        rp.res.set_start_line(
            http::status::ok, rp.req.version());
        rp.res.set_keep_alive(rp.req.keep_alive());
        rp.serializer.reset();

        // Parse the URL
        auto rv = urls::parse_uri_reference(rp.req.target());
        if (rv.has_error())
        {
            rp.status(http::status::bad_request);
            //auto [ec] = co_await rp.send("Bad Request: " + rv.error().message());
            //co_return;
        }

        rp.url = rv.value();
    }
};

http_server::
~http_server()
{
    delete impl_;
}

http_server::
http_server(
    corosio::io_context& ctx,
    std::size_t num_workers,
    http::flat_router router,
    http::shared_parser_config parser_cfg,
    http::shared_serializer_config serializer_cfg)
    : tcp_server(ctx, ctx.get_executor())
    , impl_(new impl(std::move(router)))
{
    impl_->parser_cfg = std::move(parser_cfg);
    impl_->serializer_cfg = std::move(serializer_cfg);
    wv_.reserve(num_workers);
    for(std::size_t i = 0; i < num_workers; ++i)
        wv_.emplace<worker>(ctx, this);
}

} // beast2
} // boost
