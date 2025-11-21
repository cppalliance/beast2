//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/asio_io_context.hpp>
#include <boost/beast2/server/call_mf.hpp>
#include <boost/capy/application.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/signal_set.hpp>
#include <thread>
#include <vector>

namespace boost {
namespace beast2 {

namespace {

/** Asio's io_context as an application part
*/
class asio_io_context_impl
    : public asio_io_context
{
public:
    using key_type = asio_io_context;

    ~asio_io_context_impl()
    {
    }

    asio_io_context_impl(
        capy::application& app,
        int num_threads)
        : app_(app)
        , num_threads_(num_threads)
        , ioc_(num_threads)
        , sigs_(ioc_.get_executor(), SIGINT, SIGTERM)
        , work_(ioc_.get_executor())
    {
        if(num_threads > 1)
            vt_.resize(num_threads - 1);
    }

    executor_type
    get_executor() noexcept override
    {
        return ioc_.get_executor();
    }

    std::size_t
    concurrency() const noexcept override
    {
        return num_threads_;
    }

    void attach() override
    {
        // VFALCO exception catcher?
        ioc_.run();

        // VFALCO can't figure out where to put this
        for(auto& t : vt_)
            t.join();
    }

    void start() override
    {
        // Capture SIGINT and SIGTERM to
        // perform a clean shutdown
        sigs_.async_wait(call_mf(
            &asio_io_context_impl::on_signal, this));

        for(auto& t : vt_)
        {
            t = std::thread(
                [&]
                {
                    // VFALCO exception catcher?
                    ioc_.run();
                });
        }
    }

    void stop() override
    {
        system::error_code ec;
        sigs_.cancel(ec); // VFALCO should we use the 0-arg overload?
        work_.reset();
    }

private:
    void
    on_signal(
        system::error_code const& ec, int)
    {
        if(ec == asio::error::operation_aborted)
            return;
        app_.stop();
    }

    capy::application& app_;
    int num_threads_;
    asio::io_context ioc_;
    asio::signal_set sigs_;
    asio::executor_work_guard<
        asio::io_context::executor_type> work_;
    std::vector<std::thread> vt_;
};

} // (anon)

//------------------------------------------------

asio_io_context::
~asio_io_context() = default;

auto
install_single_threaded_asio_io_context(
    capy::application& app) ->
        asio_io_context&
{
    return app.emplace<
        asio_io_context_impl>(app, 1);
}

auto
install_multi_threaded_asio_io_context(
    capy::application& app,
    int num_threads) ->
        asio_io_context&
{
    return app.emplace<
        asio_io_context_impl>(app, num_threads);
}

} // beast2
} // boost
