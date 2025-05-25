//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <cstdlib>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/http_proto/context.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/response_parser.hpp>
#include <boost/url/url_view.hpp>

struct worker
{
    using executor_type =
        boost::asio::io_context::executor_type;
    using resolver_type =
        boost::asio::ip::basic_resolver<
        boost::asio::ip::tcp, executor_type>;
    using socket_type = boost::asio::basic_stream_socket<
        boost::asio::ip::tcp, executor_type>;

    socket_type sock;
    resolver_type resolver;
    boost::http_proto::response_parser pr;
    boost::urls::url_view url;

    explicit
    worker(
        executor_type ex,
        boost::http_proto::context& ctx)
        : sock(ex)
        , resolver(ex)
        , pr(ctx)
    {
        sock.open(boost::asio::ip::tcp::v4());
    }

    void
    do_next()
    {
        do_visit("http://www.boost.org");
    }

    void
    do_visit(boost::urls::url_view url_)
    {
        url = url_;
        do_resolve();
    }

    void
    do_resolve()
    {
        resolver.async_resolve(
            url.encoded_host(),
            url.scheme(),
            [&](
                boost::system::error_code ec,
                resolver_type::results_type results)
            {
                if(ec.failed())
                {
                    // log (target, ec.message())
                    auto s = ec.message();
                    return do_next();
                }
                do_connect(results);
            });
    }

    void
    do_connect(
        resolver_type::results_type results)
    {
        boost::asio::async_connect(
            sock,
            results.begin(),
            results.end(),
            [&](
                boost::system::error_code ec,
                resolver_type::results_type::const_iterator it)
            {
                if(ec.failed())
                {
                    // log (target, ec.message())
                    return do_next();
                }
                do_request();
            });
    }

    void
    do_request()
    {
        boost::http_proto::request req;
        auto path = url.encoded_path();
        req.set_start_line(
            boost::http_proto::method::get,
            path.empty() ? "/" : path,
            boost::http_proto::version::http_1_1);

        do_shutdown();
    }

    void
    do_shutdown()
    {
        boost::system::error_code ec;
        sock.shutdown(socket_type::shutdown_both, ec);
        if(ec.failed())
        {
            // log(ec.message())
            return do_next();
        }
        do_next();
    }
};

int
main(int argc, char* argv[])
{
    boost::http_proto::context ctx;
    boost::http_proto::parser::config_base cfg;
    boost::http_proto::install_parser_service(ctx, cfg);

    boost::asio::io_context ioc;

    worker w(ioc.get_executor(), ctx);

    w.do_next();

    ioc.run();

    return EXIT_SUCCESS;
}
