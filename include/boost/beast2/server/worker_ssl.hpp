//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_WORKER_SSL_HPP
#define BOOST_BEAST2_SERVER_WORKER_SSL_HPP

#include <boost/beast2/server/http_session.hpp>
#include <boost/beast2/server/basic_router.hpp>
#include <boost/beast2/server/call_mf.hpp>
#include <boost/beast2/server/logger.hpp>
#include <boost/beast2/server/server_asio.hpp>
#include <boost/beast2/server/workers.hpp>
#include <boost/beast2/read.hpp>
#include <boost/beast2/ssl_stream.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace boost {

namespace asio {
namespace ip {
class tcp; // forward declaration
} // ip
} // asio

namespace beast2 {

template<
    class Executor,
    class Protocol = asio::ip::tcp
>
class worker_ssl : public http_session<
    ssl_stream<asio::basic_stream_socket<Protocol, Executor>>
>
{
public:
    using executor_type = Executor;
    using protocol_type = Protocol;
    using socket_type =
        asio::basic_stream_socket<Protocol, Executor>;
    using stream_type = ssl_stream<socket_type>;

    using acceptor_config = beast2::acceptor_config;

    template<
        class Executor0,
        class = typename std::enable_if<std::is_constructible<Executor, Executor0>::value>::type
    >
    worker_ssl(
        workers_base& wb,
        Executor0 const& ex,
        asio::ssl::context& ssl_ctx,
        router_asio<stream_type> rr);

    application& app() noexcept
    {
        return wb_.app();
    }

    socket_type& socket() noexcept
    {
        return stream_.next_layer();
    }

    typename Protocol::endpoint&
    endpoint() noexcept
    {
        return ep_;
    }

    /** Cancel all outstanding I/O
    */
    void cancel();

    // Called when an incoming connection is accepted
    void on_accept(acceptor_config const* pconfig);

    void on_handshake(
        acceptor_config const* pconfig,
        system::error_code const& ec);

    void on_shutdown(system::error_code ec);

    void do_fail(
        core::string_view s, system::error_code const& ec);

    void reset();

private:
    /** Called when the logical session ends
    */
    void do_close(system::error_code const& ec);

    workers_base& wb_;
    asio::ssl::context& ssl_ctx_;
    stream_type stream_;
    typename Protocol::endpoint ep_;
};

//------------------------------------------------

template<class Executor, class Protocol>
template<class Executor0, class>
worker_ssl<Executor, Protocol>::
worker_ssl(
    workers_base& wb,
    Executor0 const& ex,
    asio::ssl::context& ssl_ctx,
    router_asio<stream_type> rr)
    : http_session<stream_type>(
        wb.app(),
        stream_,
        std::move(rr),
        [this](system::error_code const& ec)
        {
            this->do_close(ec);
        })
    , wb_(wb)
    , ssl_ctx_(ssl_ctx)
    , stream_(Executor(ex), ssl_ctx)
{
}

template<class Executor, class Protocol>
void
worker_ssl<Executor, Protocol>::
cancel()
{
    system::error_code ec;
    stream_.next_layer().cancel(ec);
}

//--------------------------------------------

// Called when an incoming connection is accepted
template<class Executor, class Protocol>
void
worker_ssl<Executor, Protocol>::
on_accept(acceptor_config const* pconfig)
{
    // VFALCO TODO timeout
    using namespace std::placeholders;
    stream_.set_ssl(pconfig->is_ssl);
    if(! pconfig->is_ssl)
        return this->do_session(*pconfig);
    return stream_.stream().async_handshake(
        asio::ssl::stream_base::server,
        asio::prepend(call_mf(
            &worker_ssl::on_handshake, this), pconfig));
}

template<class Executor, class Protocol>
void
worker_ssl<Executor, Protocol>::
on_handshake(
    acceptor_config const* pconfig,
    system::error_code const& ec)
{
    if(ec.failed())
        return do_fail("worker_ssl::on_handshake", ec);

    LOG_TRC(this->sect_)(
        "{} worker_ssl::on_handshake",
        this->id());

    this->do_session(*pconfig);
}

template<class Executor, class Protocol>
void
worker_ssl<Executor, Protocol>::
on_shutdown(system::error_code ec)
{
    if(ec.failed())
        return do_fail("worker_ssl::on_shutdown", ec);

    LOG_TRC(this->sect_)(
        "{} worker_ssl::on_shutdown",
        this->id());

    stream_.next_layer().shutdown(
        asio::socket_base::shutdown_both, ec);
    // error ignored

    reset();
    wb_.do_idle(this);
}

template<class Executor, class Protocol>
void
worker_ssl<Executor, Protocol>::
do_fail(
    core::string_view s, system::error_code const& ec)
{
    reset();

    if(ec == asio::error::operation_aborted)
    {
        LOG_TRC(this->sect_)(
            "{} {}: {}",
            this->id(), s, ec.message());
        // this means the worker was stopped, don't submit new work
        return;
    }

    LOG_DBG(this->sect_)(
        "{} {}: {}",
        this->id(), s, ec.message());
    wb_.do_idle(this);
}

template<class Executor, class Protocol>
void
worker_ssl<Executor, Protocol>::
reset()
{
    // Clean up any previous connection.
    system::error_code ec;
    stream_.next_layer().close(ec);

    // asio::ssl::stream has an internal state which cannot be reset.
    // In order to perform the handshake again, we destroy the old
    // object and assign a new one, in a way that preserves the
    // original socket to avoid churning file handles.
    //
    stream_ = stream_type(std::move(stream_.next_layer()), ssl_ctx_);
}

/** Close the connection to end the session
*/
template<class Executor, class Protocol>
void
worker_ssl<Executor, Protocol>::
do_close(system::error_code const& ec)
{
    if(! ec.failed())
    {
        if(! this->pconfig_->is_ssl)
        {
            reset();
            wb_.do_idle(this);
            return;
        }
        stream_.stream().async_shutdown(call_mf(
            &worker_ssl::on_shutdown, this));
        return;
    }

    do_fail("worker_ssl::do_close", ec);
}

} // beast2
} // boost

#endif
