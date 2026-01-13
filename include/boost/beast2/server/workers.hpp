//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_WORKERS_HPP
#define BOOST_BEAST2_SERVER_WORKERS_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/log_service.hpp>
#include <boost/beast2/server/fixed_array.hpp>
#include <boost/beast2/server/router_corosio.hpp>
#include <boost/beast2/server/http_stream.hpp>
#include <boost/capy/application.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/async_run.hpp>
#include <boost/corosio/acceptor.hpp>
#include <boost/corosio/socket.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/corosio/endpoint.hpp>
#include <boost/http/server/route_handler.hpp>

#include <vector>

namespace boost {
namespace beast2 {

/** A preallocated worker that handles one HTTP connection at a time.
*/
struct http_worker
{
    corosio::socket sock;
    bool in_use = false;

    explicit http_worker(corosio::io_context& ioc)
        : sock(ioc)
    {
    }

    http_worker(http_worker&&) = default;
    http_worker& operator=(http_worker&&) = default;
};

//------------------------------------------------

/** A set of accepting sockets and their workers.

    This implements a server that accepts incoming connections
    and handles HTTP requests using preallocated workers.
*/
class workers
{
public:
    ~workers() = default;

    /** Constructor

        @param app The application which holds this server
        @param ioc The I/O context for async operations
        @param num_workers The number of workers to preallocate
        @param routes The router for dispatching requests
    */
    workers(
        capy::application& app,
        corosio::io_context& ioc,
        std::size_t num_workers,
        router_corosio routes);

    /** Add a listening endpoint

        @param config Acceptor configuration
        @param ep The endpoint to listen on
    */
    void
    listen(
        http::acceptor_config config,
        corosio::endpoint ep);

    /** Start the accept loop

        Must be called after listen() to begin accepting connections.
    */
    void start();

    /** Stop accepting and cancel all connections
    */
    void stop();

private:
    capy::task<void>
    accept_loop(corosio::acceptor& acc, http::acceptor_config config);

    capy::task<void>
    run_session(http_worker& w, http::acceptor_config const& config);

    capy::application& app_;
    corosio::io_context& ioc_;
    section sect_;
    router_corosio routes_;
    std::vector<http_worker> workers_;
    std::vector<corosio::acceptor> acceptors_;
    std::vector<http::acceptor_config> configs_;
    bool stopped_ = false;
};

//------------------------------------------------

inline
workers::
workers(
    capy::application& app,
    corosio::io_context& ioc,
    std::size_t num_workers,
    router_corosio routes)
    : app_(app)
    , ioc_(ioc)
    , sect_(use_log_service(app).get_section("workers"))
    , routes_(std::move(routes))
{
    workers_.reserve(num_workers);
    for (std::size_t i = 0; i < num_workers; ++i)
        workers_.emplace_back(ioc_);
}

inline
void
workers::
listen(
    http::acceptor_config config,
    corosio::endpoint ep)
{
    acceptors_.emplace_back(ioc_);
    acceptors_.back().listen(ep);
    configs_.push_back(config);
}

inline
void
workers::
start()
{
    stopped_ = false;
    for (std::size_t i = 0; i < acceptors_.size(); ++i)
    {
        capy::async_run(ioc_.get_executor())(
            accept_loop(acceptors_[i], configs_[i]));
    }
}

inline
void
workers::
stop()
{
    stopped_ = true;
    for (auto& acc : acceptors_)
        acc.cancel();
    for (auto& w : workers_)
    {
        if (w.in_use)
            w.sock.cancel();
    }
}

inline
capy::task<void>
workers::
accept_loop(corosio::acceptor& acc, http::acceptor_config config)
{
    while (!stopped_)
    {
        // Find a free worker
        http_worker* free_worker = nullptr;
        for (auto& w : workers_)
        {
            if (!w.in_use)
            {
                free_worker = &w;
                break;
            }
        }

        if (!free_worker)
        {
            // All workers busy - accept and immediately close
            // A production server might queue or have backpressure
            LOG_DBG(sect_)("All workers busy, rejecting connection");
            corosio::socket temp(ioc_);
            auto [ec] = co_await acc.accept(temp);
            if (ec)
            {
                if (stopped_)
                    break;
                LOG_DBG(sect_)("accept error: {}", ec.message());
            }
            temp.close();
            continue;
        }

        // Accept into the free worker's socket
        auto [ec] = co_await acc.accept(free_worker->sock);
        if (ec)
        {
            if (stopped_)
                break;
            LOG_DBG(sect_)("accept error: {}", ec.message());
            continue;
        }

        // Spawn session coroutine
        capy::async_run(ioc_.get_executor())(
            run_session(*free_worker, config));
    }
}

inline
capy::task<void>
workers::
run_session(http_worker& w, http::acceptor_config const& config)
{
    w.in_use = true;

    http_stream stream(app_, w.sock, routes_);
    co_await stream.run(config);

    w.sock.close();
    w.in_use = false;
}

} // beast2
} // boost

#endif
