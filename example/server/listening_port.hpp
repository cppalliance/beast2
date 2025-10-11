//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_LISTENING_PORT_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_LISTENING_PORT_HPP

#include "fixed_array.hpp"
#include "server.hpp"
#include <boost/asio/basic_socket_acceptor.hpp>
#include <type_traits>

namespace boost {
namespace asio {
namespace ip {
class tcp;
} // ip
} // asio
} // boost

namespace boost {
namespace http_io {

/** A TCP/IP listening port

    This implements a listening port as a server service. An array of workers
    provided upon construction is used to accept incoming connections and
    service them.

    @tparam Executor The type of executor used by the listening socket. This
    can be different from type of executor used by each worker's socket.

    @tparam Protocol Which protocol to use. This is usually `asio::ip::tcp`
*/
template< class Executor, class Protocol = asio::ip::tcp >
class listening_port : public server::part
{
public:
    using socket_type =
        asio::basic_socket_acceptor<Protocol, Executor>;
    using endpoint_type = typename Protocol::endpoint;
    using executor_type = Executor;

    /** Constructor

        @par Worker requirements
        Given:
        @li A variable `w` of type `Worker`, and
        @li A variable `s` of type `asio::basic_socket_acceptor<asio::ip::tcp, Executor>`
        these expressions must be valid:
        @code
        w.run(s);       // begin accepting incoming connections from `s`
        w.stop();       // stop accepting incoming connections from `s` and cancel all outstanding work
        @endcode

        @par Constraints
        @code
        std::is_constructible_v< Executor, Executor1 >
        @endcode

        @param srv The server associated with this listening port
        @param ep The endpoint to bind the listening port to
        @param ex The executor to use when constructing the listening port
        @param workers An array of workers used to accept connections
    */
#if 0
    template<class Executor1, class Worker
        ,typename = std::enable_if<std::is_constructible<Executor, Executor1>::value>
    >
    listening_port(
        asio_server& srv,
        fixed_array<Worker>&& workers,
        endpoint_type ep,
        Executor1 const& ex,
        bool reuse_addr = true)
        : srv_(srv)
        , sock_(ex, ep, reuse_addr)
        , wv_(std::move(workers))
        , run_(&listening_port::run_impl<Worker>)
        , stop_(&listening_port::stop_impl<Worker>)
    {
        // the workers are type-erased here to avoid
        // having too many class template parameters.
    }
#endif
    template<class Executor1
        ,typename = std::enable_if<std::is_constructible<Executor, Executor1>::value>
    >
    listening_port(
        asio_server& srv,
        endpoint_type ep,
        Executor1 const& ex,
        bool reuse_addr = true)
        : srv_(srv)
        , sock_(ex, ep, reuse_addr)
    {
    }

    socket_type&
    socket() noexcept
    {
        return sock_;
    }

    void
    run() override
    {
        (this->*run_)();
    }

    void
    stop() override
    {
        system::error_code ec;
        sock_.cancel(ec);
        (this->*stop_)();
    }

    template<
        class Worker,
        class Executor_,
        class Protocol_,
        class... Args >
    friend void
    emplace(
        listening_port<Executor_, Protocol_>& lp,
        std::size_t n,
        Args&&... args);

private:
    template<class T>
    void run_impl()
    {
        for(auto& w : wv_.to_span<T>())
            w.run();
    }

    template<class T>
    void stop_impl()
    {
        for(auto& w : wv_.to_span<T>())
            w.stop();
    }

    using mf_t = void(listening_port::*)();

    asio_server& srv_;
    socket_type sock_;
    any_fixed_array wv_;
    mf_t run_;
    mf_t stop_;
};

//------------------------------------------------

template<
    class Worker,
    class Executor,
    class Protocol,
    class... Args >
void
emplace(
    listening_port<Executor, Protocol>& lp,
    std::size_t n,
    Args&&... args)
{
    using lp_type = listening_port<Executor, Protocol>;

    fixed_array<Worker> v(n);
    while(! v.is_full())
        v.append(
            lp.srv_,
            lp.sock_,
            std::forward<Args>(args)...);
    lp.wv_ = std::move(v);
    lp.run_  = &listening_port<Executor, Protocol>::template run_impl<Worker>;
    lp.stop_ = &listening_port<Executor, Protocol>::template stop_impl<Worker>;
}

} // http_io
} // boost

#endif
