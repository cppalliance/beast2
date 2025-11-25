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
#include <boost/beast2/asio_io_context.hpp>
#include <boost/capy/application.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace beast2 {
namespace detail {
    constexpr const std::size_t worker_thread_min = 1UL;
}

namespace {

template<class Stream>
class http_server_impl
    : public http_server<Stream>
{
public:
    http_server_impl(
        capy::application& app,
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
    capy::application& app,
    http_config& config) ->
        http_server<asio::basic_stream_socket<
            asio::ip::tcp,
            asio::io_context::executor_type>>&
{
    using stream_type = asio::basic_stream_socket<
        asio::ip::tcp, asio::io_context::executor_type>;

    /// AMLALE: Default value of workers is 1.
    std::size_t num_workers = detail::worker_thread_min;

    auto nworkers = config.http_num_workers_;
    
    if (nworkers > detail::worker_thread_min)
        num_workers = nworkers;

    auto& srv = app.emplace<http_server_impl<stream_type>>(
        app, num_workers);

    auto ps_addr = config.http_addr_;
    auto ps_port = config.http_port_;

    BOOST_ASSERT(!ps_addr.empty() && (ps_port > 0));
    srv.add_port(ps_addr.c_str(), ps_port);
    
    return srv;
}

} // beast2
} // boost

