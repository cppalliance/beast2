//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/CPPAlliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_ACCEPTOR_HPP
#define BOOST_HTTP_IO_EXAMPLE_ACCEPTOR_HPP

#include "fixed_array.hpp"
#include "server.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/rts/context.hpp>
#include <string>

template< class Executor >
class worker;

template< class Executor >
class acceptor : public server::service
{
public:
    using acceptor_type = boost::asio::basic_socket_acceptor<
        boost::asio::ip::tcp, Executor >;
    using socket_type = boost::asio::basic_stream_socket<
        boost::asio::ip::tcp, Executor >;
    using executor_type = Executor;

private:
    server& srv_;
    acceptor_type sock_;
    boost::rts::context& ctx_;
    std::size_t id_ = 0;
    fixed_array< worker< executor_type > > wv_;

public:
    acceptor(
        server& srv,
        boost::asio::ip::tcp::endpoint ep,
        boost::rts::context& ctx,
        std::size_t num_workers,
        std::string const& doc_root)
        : srv_(srv)
        , sock_(srv.make_executor(), ep)
        , ctx_(ctx)
        , wv_(num_workers, srv, *this, doc_root)
    {
    }

    bool
    is_shutting_down() const noexcept
    {
        return srv_.is_shutting_down();
    }

    std::size_t
    next_id() noexcept
    {
        return ++id_;
    }

    acceptor_type&
    socket() noexcept
    {
        return sock_;
    }

    boost::rts::context&
    context() const noexcept
    {
        return ctx_;
    }

    void
    run() override
    {
        for(auto& w : wv_)
            w.run();
    }

    void
    stop() override
    {
        boost::system::error_code ec;
        sock_.cancel(ec);
        for(auto& w : wv_)
            w.stop();
    }
};

#endif
