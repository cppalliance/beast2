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
#include <boost/beast2/server/logger.hpp>
#include <boost/beast2/server/fixed_array.hpp>
#include <boost/beast2/server/server.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <utility>

namespace boost {
namespace beast2 {

class BOOST_SYMBOL_VISIBLE
    workers_base
{
public:
    BOOST_BEAST2_DECL
    virtual ~workers_base();

    virtual beast2::server& server() noexcept = 0;
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
template<
    class Executor,
    class Worker>
class workers
    : public workers_base
    , public server::part
{
public:
    using protocol_type = typename Worker::protocol_type;
    using acceptor_type =
        asio::basic_socket_acceptor<protocol_type, Executor>;
    using acceptor_config = typename Worker::acceptor_config;
    using socket_type = asio::basic_stream_socket<protocol_type, Executor>;

    /** Constructor

        @param srv The server which owns this part
        @param ex The executor to use for acceptor sockets
        @param concurrency The number of threads calling io_context::run
        @param num_workers The number of workers to construct
        @param args Arguments forwarded to each worker's constructor
    */
    template<class Executor1, class... Args>
    workers(
        beast2::server& srv,
        Executor1 const& ex,
        unsigned concurrency,
        std::size_t num_workers,
        Args&&... args);

    /** Add an acceptor
    */
    void
    emplace(
        acceptor_config&& config,
        acceptor_type&& sock)
    {
        va_.emplace_back(
            concurrency_,
            std::move(config),
            std::move(sock));
    }

private:
    struct acceptor;
    struct worker;

    void start() override;
    void stop() override;
    beast2::server& server() noexcept override;
    void do_idle(void*) override;
    void do_accepts();
    void on_accept(acceptor*, worker*,
        system::error_code const&);
    void do_stop();

    beast2::server& srv_;
    section sect_;
    Executor ex_;
    fixed_array<worker> vw_;
    std::vector<acceptor> va_;
    worker* idle_ = nullptr;
    std::size_t n_idle_ = 0;
    unsigned concurrency_;
};

} // beast2
} // boost

#include <boost/beast2/server/impl/workers.hpp>

#endif
