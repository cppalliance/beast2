//
// Copyright (c) 2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/https_server.hpp>
#include <boost/beast2/http_worker.hpp>
#include <boost/http/server/flat_router.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/cond.hpp>
#include <boost/capy/ex/strand.hpp>
#include <boost/capy/io/any_read_source.hpp>
#include <boost/capy/io/any_read_stream.hpp>
#include <boost/capy/io/any_buffer_sink.hpp>
#include <boost/corosio/tls/openssl_stream.hpp>
#include <boost/http/request_parser.hpp>
#include <boost/http/response.hpp>
#include <boost/http/server/router.hpp>
#include <boost/http/serializer.hpp>
#include <boost/http/string_body.hpp>
#include <boost/http/server/basic_router.hpp>
#include <boost/http/error.hpp>
#include <boost/url/parse.hpp>
#include <iostream>
#include <memory>

namespace boost {
namespace beast2 {

struct https_server::impl
{
    corosio::tls::context tls_ctx;
    http::flat_router router;
    http::shared_parser_config parser_cfg;
    http::shared_serializer_config serializer_cfg;

    impl(
        corosio::tls::context tc,
        http::flat_router r)
        : tls_ctx(std::move(tc))
        , router(std::move(r))
    {
    }
};

struct https_server::
    worker
    : tcp_server::worker_base
    , http_worker
{
    corosio::io_context& ctx;
    capy::strand<corosio::io_context::executor_type> strand;
    corosio::socket sock;
    corosio::tls::context tls_ctx;
    std::unique_ptr<corosio::openssl_stream> ssl;

    worker(
        corosio::io_context& ctx_,
        https_server* srv_)
        : http_worker(
            srv_->impl_->router,
            srv_->impl_->parser_cfg,
            srv_->impl_->serializer_cfg)
        , ctx(ctx_)
        , strand(ctx_.get_executor())
        , sock(ctx_)
        , tls_ctx(srv_->impl_->tls_ctx)
    {
        sock.open();
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
        // Create TLS stream wrapping the socket
        ssl = std::make_unique<corosio::openssl_stream>(sock, tls_ctx);

        // Perform TLS handshake as server
        auto [hs_ec] = co_await ssl->handshake(corosio::tls_stream::server);
        if(hs_ec)
        {
            std::cerr << "TLS handshake error: " << hs_ec.message() << "\n";
            sock.shutdown(corosio::socket::shutdown_both);
            ssl.reset();
            co_return;
        }

        // Wire parser and serializer to the TLS stream
        rp.req_body = capy::any_buffer_source(parser.source_for(*ssl));
        rp.res_body = capy::any_buffer_sink(serializer.sink_for(*ssl));
        stream = capy::any_read_stream(ssl.get());

        // Process HTTP requests over TLS
        co_await do_http_session();

        // Perform TLS shutdown
        auto [shut_ec] = co_await ssl->shutdown();
        if(shut_ec)
        {
            // TLS shutdown errors are common (peer may close abruptly)
        }

        // Clean up TLS stream before TCP shutdown
        ssl.reset();

        sock.shutdown(corosio::socket::shutdown_both);
    }
};

https_server::
~https_server()
{
    delete impl_;
}

https_server::
https_server(
    corosio::io_context& ctx,
    std::size_t num_workers,
    corosio::tls::context tls_ctx,
    http::flat_router router,
    http::shared_parser_config parser_cfg,
    http::shared_serializer_config serializer_cfg)
    : tcp_server(ctx, ctx.get_executor())
    , impl_(new impl(std::move(tls_ctx), std::move(router)))
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
