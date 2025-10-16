//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_WORKER_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_WORKER_HPP

#include "asio_server.hpp"
#include "handler.hpp"
#include "http_responder.hpp"
#include "logger.hpp"
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <stddef.h>
#include <string>

#include <functional> // for _1

namespace boost {

namespace asio {
namespace ip {
class tcp; // forward declaration
} // ip
} // asio

namespace http_io {

//------------------------------------------------

template<
    class Executor,
    class Executor1 = Executor,
    class Protocol = asio::ip::tcp
>
class worker : public http_responder<worker<Executor, Executor1, Protocol>>
{
public:
    using executor_type = Executor;

    using protocol_type = Protocol;

    using acceptor_type =
        asio::basic_socket_acceptor<Protocol, Executor1>;

    using socket_type =
        asio::basic_stream_socket<Protocol, Executor>;

    using stream_type =
        asio::basic_stream_socket<Protocol, Executor>;

    template<
        class Executor_
        , class = typename std::enable_if<std::is_constructible<Executor, Executor_>::value>::type
    >
    worker(
        asio_server& srv,
        acceptor_type& acc,
        Executor_ const& ex,
        router_type& rr)
        : http_responder<worker>(srv, rr)
        , acc_(acc)
        , sock_(ex)
    {
    }

    stream_type&
    stream() noexcept
    {
        return sock_;
    }

    /** Run the worker I/O loop

        The worker will continue to run until
        @ref stop is called or the worker is destroyed.
    */
    void run()
    {
        do_accept();
    }

    /** Stop the worker

        Any outstanding I/O is canceled.
    */
    void stop()
    {
        system::error_code ec;
        sock_.cancel(ec);
    }

    /** Called when the connected session is ended
    */
    void do_close()
    {
        system::error_code ec;
        sock_.shutdown(asio::socket_base::shutdown_both, ec);
        // error ignored

        return do_accept();
    }

    void do_failed(
        core::string_view s, system::error_code const& ec)
    {
        if(ec == asio::error::operation_aborted)
        {
            LOG_TRC(sect_, this->id(), " ", s, ": operation aborted");
            // this means the worker was stopped, don't submit new work
            return;
        }

        LOG_DBG(sect_, this->id(), " ", s, ": ", ec.message());
        return do_accept();
    }

private:
    void do_accept()
    {
        system::error_code ec;
        sock_.close(ec); // error ignored
        
        acc_.async_accept(sock_, ep_, call_mf(
            &worker::on_accept, this));
    }

    void on_accept(system::error_code const& ec)
    {
        if(ec.failed())
            return do_failed("worker::on_accept", ec);

        // Request must be fully processed within 60 seconds.
        //request_deadline_.expires_after(
            //std::chrono::seconds(60));
        this->do_start();
    }

private:
    // order of destruction matters here
    acceptor_type& acc_;
    section sect_;
    socket_type sock_;
    typename Protocol::endpoint ep_;
};

} // http_io
} // boost

#endif
