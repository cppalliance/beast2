//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/server_asio.hpp>
#include <boost/beast2/server/call_mf.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <thread>
#include <vector>

namespace boost {
namespace beast2 {

struct server_asio::impl
{
    using strand_type =
        asio::strand<asio::io_context::executor_type>;

    impl(
        application& app_,
        int num_threads_)
        : app(app_)
        , num_threads(num_threads_)
        , ioc(num_threads_)
        , sigs(ioc.get_executor(), SIGINT, SIGTERM)
        , work(ioc.get_executor())
    {
        if(num_threads > 1)
            vt.resize(num_threads - 1);
    }

    application& app;
    int num_threads;
    asio::io_context ioc;
    asio::signal_set sigs;
    std::vector<std::thread> vt;
    asio::executor_work_guard<
        asio::io_context::executor_type> work;
};

server_asio::
~server_asio()
{
    delete impl_;
}

server_asio::
server_asio(
    application& app,
    int num_threads)
    : impl_(new impl(app, num_threads))
{
}

auto
server_asio::
get_executor() noexcept ->
    executor_type
{
    return impl_->ioc.get_executor();
}

void
server_asio::
start()
{
    // Capture SIGINT and SIGTERM to
    // perform a clean shutdown
    impl_->sigs.async_wait(call_mf(
        &server_asio::on_signal, this));

    for(auto& t : impl_->vt)
    {
        t = std::thread(
            [&]
            {
                // VFALCO exception catcher?
                impl_->ioc.run();
            });
    }
}

void
server_asio::
stop()
{
    system::error_code ec;
    impl_->sigs.cancel(ec); // VFALCO should we use the 0-arg overload?
    impl_->work.reset();
}

void
server_asio::
on_signal(
    system::error_code const& ec, int)
{
    if(ec == asio::error::operation_aborted)
        return;
    impl_->app.stop();
}

void
server_asio::
attach()
{
    // VFALCO exception catcher?
    impl_->ioc.run();

    // VFALCO can't figure out where to put this
    for(auto& t : impl_->vt)
        t.join();
}

} // beast2
} // boost
