//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_PLAIN_WORKER_HPP
#define BOOST_BEAST2_SERVER_PLAIN_WORKER_HPP

#include <boost/beast2/server/http_stream.hpp>
#include <boost/beast2/server/call_mf.hpp>
#include <boost/beast2/server/router_asio.hpp>
#include <boost/beast2/server/workers.hpp>
#include <boost/beast2/logger.hpp>
#include <boost/beast2/read.hpp>
#include <boost/rts/application.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/prepend.hpp>

namespace boost {
namespace beast2 {

template<class Executor, class Protocol>
class plain_worker
    : public http_stream<
        asio::basic_stream_socket<Protocol, Executor>
    >
{
public:
    using executor_type = Executor;
    using protocol_type = Protocol;
    using socket_type =
        asio::basic_stream_socket<Protocol, Executor>;
    using stream_type = socket_type;
    using acceptor_config = beast2::acceptor_config;

    template<class Executor0>
    plain_worker(
        workers_base& wb,
        Executor0 const& ex,
        router_asio<stream_type&> rr);

    rts::application& app() noexcept
    {
        return wb_.app();
    }

    socket_type& socket() noexcept
    {
        return stream_;
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

    void do_fail(
        core::string_view s, system::error_code const& ec);

    void reset();

private:
    /** Called when the logical session ends
    */
    void do_close(system::error_code const& ec);

    workers_base& wb_;
    stream_type stream_;
    typename Protocol::endpoint ep_;
};

//------------------------------------------------

template<class Executor, class Protocol>
template<class Executor0>
plain_worker<Executor, Protocol>::
plain_worker(
    workers_base& wb,
    Executor0 const& ex,
    router_asio<stream_type&> rr)
    : http_stream<stream_type>(
        wb.app(),
        stream_,
        std::move(rr),
        [this](system::error_code const& ec)
        {
            this->do_close(ec);
        })
    , wb_(wb)
    , stream_(Executor(ex))
{
}

template<class Executor, class Protocol>
void
plain_worker<Executor, Protocol>::
cancel()
{
    system::error_code ec;
    stream_.cancel(ec);
}

//--------------------------------------------

// Called when an incoming connection is accepted
template<class Executor, class Protocol>
void
plain_worker<Executor, Protocol>::
on_accept(acceptor_config const* pconfig)
{
    BOOST_ASSERT(stream_.get_executor().running_in_this_thread());
    // VFALCO TODO timeout
    this->on_stream_begin(*pconfig);
}

template<class Executor, class Protocol>
void
plain_worker<Executor, Protocol>::
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
plain_worker<Executor, Protocol>::
reset()
{
    // Clean up any previous connection.
    system::error_code ec;
    stream_.close(ec);
}

/** Close the connection to end the session
*/
template<class Executor, class Protocol>
void
plain_worker<Executor, Protocol>::
do_close(system::error_code const& ec)
{
    if(! ec.failed())
    {
        reset();
        wb_.do_idle(this);
        return;
    }

    do_fail("plain_worker::do_close", ec);
}

} // beast2
} // boost

#endif
