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
#include <boost/asio/basic_waitable_timer.hpp>
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

    explicit
    impl(
        int num_threads_)
        : num_threads(num_threads_)
        , ioc(num_threads_)
        , sigs(ioc.get_executor(), SIGINT, SIGTERM)
        , timer(strand_type(ioc.get_executor()))
    {
    }

    int num_threads;
    asio::io_context ioc;
    asio::signal_set sigs;
    asio::basic_waitable_timer<
        std::chrono::steady_clock,
        asio::wait_traits<std::chrono::steady_clock>,
        strand_type> timer;
    std::vector<std::thread> vt;
    bool got_sigint = false;
};

server_asio::
~server_asio()
{
    delete impl_;
}

server_asio::
server_asio(
    int num_threads)
    : impl_(new impl(num_threads))
{
    if( impl_->num_threads > 1)
        impl_->vt.resize(impl_->num_threads - 1);
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
run()
{
    do_start();

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
    // VFALCO exception catcher?
    impl_->ioc.run();
}

void
server_asio::
stop()
{
    if(is_stopping())
    {
        // happens when SIGINT and timer both fire
        return;
    }
    system::error_code ec;
    impl_->sigs.cancel(ec); // VFALCO should we use the 0-arg overload?
    impl_->timer.cancel();
    do_stop();
    // is_stopping() still returns `true` here
    for(auto& t : impl_->vt)
        t.join();
}

void
server_asio::
on_signal(
    system::error_code const&, int)
{
    if(impl_->got_sigint)
    {
        // second SIGINT causes immediate stop
        return stop();
    }

    // first SIGINT starts a 30 second shutdown
    impl_->got_sigint = true;
    impl_->sigs.async_wait(call_mf(
        &server_asio::on_signal, this));
    impl_->timer.expires_after(std::chrono::seconds(30));
    impl_->timer.async_wait(call_mf(
        &server_asio::on_timer, this));
}

void
server_asio::
on_timer(
    boost::system::error_code const& ec)
{
    if(! ec.failed())
        return stop();
    if(ec != boost::asio::error::operation_aborted)
    {
        // log?
    }
}

} // beast2
} // boost
