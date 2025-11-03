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
#include <boost/beast2/application.hpp>
#include <boost/beast2/server/logger.hpp>
#include <boost/beast2/server/fixed_array.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>

namespace boost {
namespace beast2 {

class BOOST_SYMBOL_VISIBLE
    workers_base
{
public:
    BOOST_BEAST2_DECL
    virtual ~workers_base();

    virtual application& app() noexcept = 0;
    virtual void do_idle(void* worker) = 0;
};

/** A set of accepting sockets and their workers.

    This implements a set of listening ports as a server service. An array of
    workers created upon construction is used to accept incoming connections
    and handle their sessions.

    @par Worker exemplar
    @code
    template< class Executor >
    struct Worker
    {
        using executor_type = Executor;
        using protocol_type = asio::ip::tcp;
        using socket_type = asio::basic_stream_socket<protocol_type, Executor>;
        using acceptor_config = beast2::acceptor_config;

        asio::basic_stream_socket<protocol_type, Executor>& socket() noexcept;
        typename protocol_type::endpoint& endpoint() noexcept;

        void on_accept();
    };
    @endcode

    @tparam Executor The type of executor used by acceptor sockets.
    @tparam Worker The type of worker to use.
*/
template<class Worker>
class workers
    : public workers_base
{
public:
    using executor_type = typename Worker::executor_type;
    using protocol_type = typename Worker::protocol_type;
    using acceptor_type = asio::basic_socket_acceptor<protocol_type, executor_type>;
    using acceptor_config = typename Worker::acceptor_config;
    using socket_type = asio::basic_stream_socket<protocol_type, executor_type>;

    ~workers()
    {
    }

    /** Constructor

        @param app The @ref application which holds this part
        @param ex The executor to use for acceptor sockets
        @param concurrency The number of threads calling io_context::run
        @param num_workers The number of workers to construct
        @param args Arguments forwarded to each worker's constructor
    */
    template<class Executor1, class... Args>
    workers(
        application& app,
        Executor1 const& ex,
        unsigned concurrency,
        std::size_t num_workers,
        Args&&... args);

    /** Add an acceptor
    */
    template<class... Args>
    void
    emplace(
        acceptor_config&& config,
        Args&&... args)
    {
        va_.emplace_back(
            concurrency_,
            std::move(config),
            acceptor_type(ex_,
                std::forward<Args>(args)...));
    }

    void start();
    void stop();

private:
    struct acceptor;
    struct worker;

    application& app() noexcept override;
    void do_idle(void*) override;
    void do_accepts();
    void on_accept(acceptor*, worker*,
        system::error_code const&);
    void do_stop();

    application& app_;
    section sect_;
    executor_type ex_;
    fixed_array<worker> vw_;
    std::vector<acceptor> va_;
    worker* idle_ = nullptr;
    std::size_t n_idle_ = 0;
    unsigned concurrency_;
    bool stop_ = false;
};

//------------------------------------------------

template<class Worker>
struct workers<Worker>::
    acceptor
{
    template<class... Args>
    explicit
    acceptor(
        std::size_t concurrency,
        acceptor_config&& config_,
        acceptor_type&& sock_)
        : config(std::move(config_))
        , sock(std::move(sock_))
        , need(concurrency)
    {
    }

    acceptor_config config;
    asio::basic_socket_acceptor<
        protocol_type, executor_type> sock;
    std::size_t need;   // number of accepts we need
};

template<class Worker>
struct workers<Worker>::
    worker
{
    worker* next;
    Worker w;

    template<class... Args>
    explicit worker(
        worker* next_, Args&&... args)
        : next(next_)
        , w(std::forward<Args>(args)...)
    {
    }
};

//------------------------------------------------

template<class Worker>
template<class Executor1, class... Args>
workers<Worker>::
workers(
    application& app,
    Executor1 const& ex,
    unsigned concurrency,
    std::size_t num_workers,
    Args&&... args)
    : app_(app)
    , sect_(app_.sections().get("workers"))
    , ex_(executor_type(ex))
    , vw_(num_workers)
    , concurrency_(concurrency)
{
    while(! vw_.is_full())
        idle_ = &vw_.emplace_back(idle_, *this,
            std::forward<Args>(args)...);
    n_idle_ = vw_.size();
}

template<class Worker>
application&
workers<Worker>::
app() noexcept
{
    return app_;
}

template<class Worker>
void
workers<Worker>::
do_idle(void* pw)
{
    asio::dispatch(ex_,
        [this, pw]()
        {
            // recover the `worker` pointer without using offsetof
            worker* w = vw_.data() + (
                reinterpret_cast<std::uintptr_t>(pw) -
                reinterpret_cast<std::uintptr_t>(vw_.data())) /
                sizeof(worker);
            // push
            w->next = idle_;
            idle_ = w;
            ++n_idle_;
            do_accepts();
        });
}
template<class Worker>
void
workers<Worker>::
start()
{
    asio::dispatch(ex_, call_mf(&workers::do_accepts, this));
}

template<class Worker>
void
workers<Worker>::
stop()
{
    asio::dispatch(ex_, call_mf(&workers::do_stop, this));
}

template<class Worker>
void
workers<Worker>::
do_accepts()
{
    BOOST_ASSERT(ex_.running_in_this_thread());
    if(stop_)
        return;
    if(idle_)
    {
        for(auto& a : va_)
        {
            while(a.need > 0)
            {
                --a.need;
                // pop
                auto pw = idle_;
                idle_ = idle_->next;
                --n_idle_;
                a.sock.async_accept(pw->w.socket(), pw->w.endpoint(),
                    asio::prepend(call_mf(&workers::on_accept, this), &a, pw));
                if(! idle_)
                    goto busy;
            }
        }
        return;
    }
busy:
    // all workers are busy
    // VFALCO log to warn the server admin?
    return;
}

template<class Worker>
void
workers<Worker>::
on_accept(
    acceptor* pa,
    worker* pw,
    system::error_code const& ec)
{
    BOOST_ASSERT(ex_.running_in_this_thread());
    ++pa->need;
    if(ec.failed())
    {
        // push
        pw->next = idle_;
        idle_ = pw;
        ++n_idle_;
        LOG_DBG(sect_)("async_accept: {}", ec.message());
        return do_accepts();
    }
    do_accepts();
    asio::dispatch(pw->w.socket().get_executor(), asio::prepend(
        call_mf(&Worker::on_accept, &pw->w), &pa->config));
}

template<class Worker>
void
workers<Worker>::
do_stop()
{
    stop_ = true;

    for(auto& a : va_)
    {
        system::error_code ec;
        a.sock.cancel(ec); // error ignored
    }
    for(auto& w : vw_)
        w.w.cancel();
}

} // beast2
} // boost

#endif
