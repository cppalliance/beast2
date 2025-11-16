//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/http_server.hpp>
#include <boost/beast2/server/http_stream.hpp>
#include <boost/beast2/server/plain_worker.hpp>
#include <boost/beast2/application.hpp>
#include <boost/beast2/asio_io_context.hpp>

namespace boost {
namespace beast2 {

namespace {

template<class Stream>
class http_server_impl
    : public http_server<Stream>
{
public:
    http_server_impl(
        application& app,
        std::size_t num_workers)
        : ioc_(install_single_threaded_asio_io_context(app))
        , w_(app,
            ioc_.get_executor(), 1, num_workers,
            ioc_.get_executor(), this->wwwroot)
    {
    }

    void add_port(
        char const* addr,
        unsigned short port)
    {
        w_.emplace(
            acceptor_config{ false, false },
            asio::ip::tcp::endpoint(
                asio::ip::make_address(addr),
                port),
            true);
    }

    void start()
    {
        w_.start();
    }

    void stop()
    {
        w_.stop();
    }

    void attach() override
    {
        ioc_.attach();
    }

private:
    using workers_type = workers< plain_worker<
        asio::io_context::executor_type, asio::ip::tcp> >;

    asio_io_context& ioc_;
    workers_type w_;
};

} // (anon)

//------------------------------------------------

auto
install_plain_http_server(
    application& app,
    char const* addr,
    unsigned short port,
    std::size_t num_workers) ->
        http_server<asio::basic_stream_socket<
            asio::ip::tcp,
            asio::io_context::executor_type>>&
{
    using stream_type = asio::basic_stream_socket<
        asio::ip::tcp, asio::io_context::executor_type>;
    auto& srv = app.emplace<http_server_impl<stream_type>>(
        app, num_workers);
    srv.add_port(addr, port);
    return srv;
}

} // beast2
} // boost

