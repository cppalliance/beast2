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
#include <boost/capy/io/any_read_source.hpp>
#include <boost/capy/io/any_read_stream.hpp>
#include <boost/capy/io/any_buffer_sink.hpp>
#include <boost/http/request_parser.hpp>
#include <boost/http/response.hpp>
#include <boost/http/server/router.hpp>
#include <boost/http/serializer.hpp>
#include <boost/http/string_body.hpp>
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

class http_server::http_worker
{
public:
    http::flat_router fr;
    http::route_params rp;
    capy::any_read_stream stream;
    http::request_parser parser;
    http::serializer serializer;

    http_worker(
        http::flat_router fr_,
        http::shared_parser_config parser_cfg,
        http::shared_serializer_config serializer_cfg)
        : fr(std::move(fr_))
        , parser(parser_cfg)
        , serializer(serializer_cfg)
    {
        serializer.set_message(rp.res);
    }

    capy::task<void>
    do_http_session(
        corosio::socket& stream)
    {
        struct guard
        {
            http_worker& self;

            guard(http_worker& self_)
                : self(self_)
            {
            }

            ~guard()
            {
                self.parser.reset();
                self.parser.start();
                self.rp.session_data.clear();
            }
        };

        guard g(*this); // clear things when session ends

        // read request, send response loop
        for(;;)
        {
            parser.reset();
            parser.start();
            rp.session_data.clear();

            // Read HTTP request header
            //auto [ec, n] = co_await w.read_header();
            auto [ec] = co_await parser.read_header( stream );
            if (ec)
            {
                std::cerr << "read_header error: " << ec.message() << "\n";
                break;
            }

            // Process headers and dispatch
            // Set up Request and Response objects
            rp.req = parser.get();
            rp.route_data.clear();
            rp.res.set_start_line(
                http::status::ok, rp.req.version());
            rp.res.set_keep_alive(rp.req.keep_alive());
            serializer.reset();

            // Parse the URL
            {
                auto rv = urls::parse_uri_reference(rp.req.target());
                if (rv.has_error())
                {
                    rp.status(http::status::bad_request);
                    //auto [ec] = co_await rp.send("Bad Request: " + rv.error().message());
                    //co_return;
                }
                rp.url = rv.value();
            }

            {
                auto rv = co_await fr.dispatch(rp.req.method(), rp.url, rp);
                if(rv.failed())
                {
                    // VFALCO log rv.error()
                    break;
                }

                if(! rp.res.keep_alive())
                    break;
            }
        }
    }
};

// Each worker owns its own socket and parser/serializer state,
// allowing concurrent connection handling without synchronization.
struct http_server::
    worker
    : tcp_server::worker_base
    , http_worker
{
    corosio::io_context& ctx;
    capy::strand<corosio::io_context::executor_type> strand;
    corosio::socket sock;

    worker(
        corosio::io_context& ctx_,
        http_server* srv_)
        : http_worker(
            srv_->impl_->router,
            srv_->impl_->parser_cfg,
            srv_->impl_->serializer_cfg)
        , ctx(ctx_)
        , strand(ctx_.get_executor())
        , sock(ctx_)
    {
        sock.open();

        rp.req_body = capy::any_buffer_source(parser.source_for(sock));
        rp.res_body = capy::any_buffer_sink(serializer.sink_for(sock));
        stream = capy::any_read_stream(&sock);
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
        co_await do_http_session(sock);

        sock.shutdown(corosio::socket::shutdown_both); // VFALCO too wordy
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

    std::vector<std::unique_ptr<tcp_server::worker_base>> workers;
    workers.reserve(num_workers);
    for(std::size_t i = 0; i < num_workers; ++i)
        workers.push_back(std::make_unique<worker>(ctx, this));
    set_workers(std::move(workers));
}

} // beast2
} // boost
