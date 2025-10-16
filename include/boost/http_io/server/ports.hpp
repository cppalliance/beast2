//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_PORTS_HPP
#define BOOST_HTTP_IO_SERVER_PORTS_HPP

#include "logger.hpp"
#include <boost/http_io/detail/config.hpp>
#include <boost/http_io/server/call_mf.hpp>
#include <boost/http_io/server/fixed_array.hpp>
#include <boost/http_io/server/server.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/dispatch.hpp>
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

template<class T>
struct of_type_t
{
    using type = T;
};

namespace detail {
template<class T>
struct of_type_impl
{
    static of_type_t<T> const value;
};
template<class T>
of_type_t<T> const
of_type_impl<T>::value = of_type_t<T>{};
} // detail

template<class T>
constexpr of_type_t<T> const& of_type =
    detail::of_type_impl<T>::value;

class ports_base
{
public:
    struct entry
    {
        std::size_t need;   // number of accepts we need
    };

    struct api
    {
        std::size_t index;
    };

};

/** A TCP/IP listening port

    This implements a listening port as a server service. An array of workers
    provided upon construction is used to accept incoming connections and
    service them.

    @tparam Executor The type of executor used by the listening socket. This
    can be different from type of executor used by each worker's socket.

    @tparam Protocol Which protocol to use. This is usually `asio::ip::tcp`
*/
template< class Executor, class Protocol = asio::ip::tcp >
class ports
    : public ports_base
    , public server::part
{
public:
    using executor_type = Executor;
    using protocol_type = Protocol;
    using endpoint = typename Protocol::endpoint;
    using acceptor = asio::basic_socket_acceptor<Protocol, Executor>;

    struct entry_impl : entry
    {
        template<class... Args>
        explicit entry_impl(Args&&... args)
            : sock(std::forward<Args>(args)...)
        {
        }

        acceptor sock;
    };

    /** Constructor

        @param concurrency The number of threads calling io_context::run
    */
    template<
        class Executor1,
        class Worker,
        class... Args,
        class = typename std::enable_if<std::is_constructible<Executor, Executor1>::value>::type
    >
    ports(
        server&,
        Executor1 const& ex,
        unsigned concurrency,
        std::size_t num_workers,
        of_type_t<Worker>,
        Args&&... args)
        : ex_(ex)
        , concurrency_(concurrency)
    {
        fixed_array<Worker> vw(num_workers);
        while(! vw.is_full())
            vw.emplace_back(
                vw.size() + 1,
                [this](std::size_t i)
                {
                    asio::dispatch(ex_,
                        [this, i]
                        {
                            do_idle(i);
                        });
                },
                std::forward<Args>(args)...);
        vw_ = std::move(vw);

        // tracks linked list of idle workers
        vi_.resize(num_workers);
        for(std::size_t i = 0; i < vi_.size() - 1; ++i)
            vi_[i] = i + 2;
        vi_.back() = 0;

        do_accept_ = &ports::do_worker_accept<Worker>;
        do_stop_ = &ports::do_worker_stop<Worker>;
    }

    template<class Worker>
    void do_worker_accept(std::size_t i, entry_impl& e)
    {
        vw_.to_span<Worker>()[i - 1].do_accept(e.sock,
            [this, &e]
            {
                asio::dispatch(ex_,
                    [this, &e]
                    {
                        ++e.need;
                        LOG_TRC(sect_, "need=", e.need);
                    });
            });
    }

    template<class Worker>
    void do_worker_stop()
    {
        for(auto& w : vw_.to_span<Worker>())
            w.cancel();
        for(auto& e : ve_)
        {
            system::error_code ec;
            e.sock.cancel(ec);
        }
    }

    void do_idle(std::size_t index)
    {
        vi_[index - 1] = idle_;
        idle_ = index;
        do_accept();
    }

    /** Add another port
    */
    void emplace(
        endpoint const& ep,
        bool reuse_addr = true)
    {
        ve_.emplace_back(ex_, ep, reuse_addr);
        ve_.back().need = concurrency_;
    }

private:
    void run() override
    {
        asio::dispatch(ex_, call_mf(&ports::do_accept, this));
    }

    void stop() override
    {
        asio::dispatch(ex_, call_mf(&ports::do_stop, this));
    }

    void do_accept()
    {
        if(idle_ == 0) // no idle workers
            return;
        for(auto& e : ve_)
        {
            while(e.need > 0)
            {
                --e.need;
                LOG_TRC(sect_, "need=", e.need);
                auto const i = idle_;
                idle_ = vi_[idle_ - 1];
                (this->*do_accept_)(i, e);
                if(idle_ == 0) // no idle workers
                    return;
            }
        }
    }

    void do_stop()
    {
        (this->*do_stop_)();
    }

    section sect_;
    Executor ex_;
    unsigned concurrency_;
    std::vector<entry_impl> ve_;
    any_fixed_array vw_;
    std::vector<std::size_t> vi_;
    std::size_t idle_ = 1;

    void(ports::*do_accept_)(std::size_t, entry_impl&);
    void(ports::*do_stop_)();
};

} // http_io
} // boost

#endif
