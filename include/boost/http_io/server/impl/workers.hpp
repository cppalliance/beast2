//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_IMPL_WORKERS_HPP
#define BOOST_HTTP_IO_SERVER_IMPL_WORKERS_HPP

#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>

namespace boost {
namespace http_io {

template<class Executor, class Worker>
struct workers<Executor, Worker>::
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
        protocol_type, Executor> sock;
    std::size_t need;   // number of accepts we need
};

template<class Executor, class Worker>
struct workers<Executor, Worker>::
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

template<class Executor, class Worker>
template<class Executor1, class... Args>
workers<Executor, Worker>::
workers(
    http_io::server& srv,
    Executor1 const& ex,
    unsigned concurrency,
    std::size_t num_workers,
    Args&&... args)
    : srv_(srv)
    , ex_(ex)
    , vw_(num_workers)
    , concurrency_(concurrency)
{
    while(! vw_.is_full())
        idle_ = &vw_.emplace_back(idle_, *this,
            std::forward<Args>(args)...);
    n_idle_ = vw_.size();
}

template<class Executor, class Worker>
http_io::server&
workers<Executor, Worker>::
server() noexcept
{
    return srv_;
}

template<class Executor, class Worker>
void
workers<Executor, Worker>::
do_idle(void* pw)
{
    asio::dispatch(ex_,
        [this, pw]()
        {
            worker* w = vw_.data() +
                reinterpret_cast<std::uintptr_t>(pw) /
                reinterpret_cast<std::uintptr_t>(vw_.data());
            // push
            w->next = idle_;
            idle_ = w;
            ++n_idle_;
            do_accepts();
        });
}
template<class Executor, class Worker>
void
workers<Executor, Worker>::
start()
{
    asio::dispatch(ex_, call_mf(&workers::do_accepts, this));
}

template<class Executor, class Worker>
void
workers<Executor, Worker>::
stop()
{
    asio::dispatch(ex_, call_mf(&workers::do_stop, this));
}

template<class Executor, class Worker>
void
workers<Executor, Worker>::
do_accepts()
{
    BOOST_ASSERT(ex_.running_in_this_thread());
    if(srv_.is_stopping())
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

template<class Executor, class Worker>
void
workers<Executor, Worker>::
on_accept(
    acceptor* pa,
    worker* pw,
    system::error_code const& ec)
{
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

template<class Executor, class Worker>
void
workers<Executor, Worker>::
do_stop()
{
    for(auto& a : va_)
    {
        system::error_code ec;
        a.sock.cancel(ec); // error ignored
    }
    for(auto& w : vw_)
        w.w.cancel();
}

} // http_io
} // boost

#endif
