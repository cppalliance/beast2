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

#include <boost/http_io/server/logger.hpp>
#include <boost/http_io/detail/config.hpp>
#include <boost/http_io/server/any_lambda.hpp>
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

class BOOST_SYMBOL_VISIBLE ports_base
{
public:
    BOOST_HTTP_IO_DECL
    virtual ~ports_base();

    virtual void do_idle(void* worker) = 0;
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
    using socket_type = asio::basic_stream_socket<Protocol, Executor>;

    struct entry
    {
        template<class... Args>
        explicit
        entry(
            unsigned concurrency,
            Args&&... args)
            : sock(std::forward<Args>(args)...)
            , need(concurrency)
        {
            need = concurrency;
        }

        acceptor sock;
        std::size_t need;   // number of accepts we need
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
        , do_idle_(&ports::do_idle_worker<Worker>)
    {
        fixed_array<Worker> vw(num_workers);

        while(! vw.is_full())
            vw.emplace_back(
                *this,
                std::forward<Args>(args)...);
        vw_ = std::move(vw);

        // tracks linked list of idle workers
        vi_.resize(num_workers);
        for(std::size_t i = 0; i < vi_.size() - 1; ++i)
            vi_[i] = i + 2;
        vi_.back() = 0;
        n_idle_ = vi_.size();

        do_accept_ = &ports::do_accept<Worker>;
        do_stop_ = &ports::do_stop<Worker>;
    }

    void do_idle(void* worker) override
    {
        asio::dispatch(ex_,
            [this, worker]()
            {
                (this->*do_idle_)(worker);
            });
    }

    template<class Worker>
    void do_idle_worker(void* pw)
    {
        auto const i = (reinterpret_cast<Worker*>(pw) -
            this->vw_.to_span<Worker>().data()) + 1;
        // push idle worker
        vi_[i - 1] = idle_;
        idle_ = i;
        ++n_idle_;
        do_accepts();
    }

    /** Add an acceptor bound to the specified endpoint

        @param ep The endpoint to bind the acceptor to.
        @param reuse_addr If `true`, set the `SO_REUSEADDR` socket option on the acceptor.
    */
    void emplace(
        endpoint const& ep,
        bool reuse_addr = true)
    {
        ve_.emplace_back(concurrency_, ex_, ep, reuse_addr);
    }

private:
    void run() override
    {
        asio::dispatch(ex_, call_mf(&ports::do_accepts, this));
    }

    void stop() override
    {
        asio::dispatch(ex_,
            [&]
            {
                (this->*do_stop_)();
            });
    }

    void do_accepts()
    {
        BOOST_ASSERT(ex_.running_in_this_thread());
        if(idle_ == 0) // no idle workers
            return;
        for(auto& e : ve_)
        {
            while(e.need > 0)
            {
                --e.need;
                LOG_TRC(sect_, "need=", e.need);
                // pop idle worker
                auto const i = idle_;
                idle_ = vi_[idle_ - 1];
                --n_idle_;
                LOG_TRC(sect_, "n_idle=", n_idle_);
                (this->*do_accept_)(i, e);
                if(idle_ == 0) // no idle workers
                    return;
            }
        }
    }

    template<class Worker>
    void do_accept(std::size_t i, entry& e)
    {
        BOOST_ASSERT(ex_.running_in_this_thread());
        auto& w = vw_.to_span<Worker>()[i - 1];
        e.sock.async_accept(w.socket(), w.endpoint(),
            [this, i, &e](system::error_code const& ec)
            {
                ++e.need;
                LOG_TRC(sect_, "need=", e.need);
                if(ec.failed())
                {
                    // push idle worker
                    vi_[i - 1] = idle_;
                    idle_ = i;
                    ++n_idle_;
                    LOG_TRC(sect_, "n_idle=", n_idle_);
                    LOG_DBG(sect_, "async_accept: ", ec.message());
                    return do_accepts();
                }
                do_accepts();
                auto& w = vw_.to_span<Worker>()[i - 1];
                asio::dispatch(w.socket().get_executor(),
                    call_mf(&Worker::on_accept, &w));
            });
    }

    template<class Worker>
    void do_stop()
    {
        for(auto& w : vw_.to_span<Worker>())
            w.cancel();
        for(auto& e : ve_)
        {
            system::error_code ec;
            e.sock.cancel(ec); // error ignored
        }
    }

    std::size_t n_idle_ = 0;
    section sect_;
    Executor ex_;
    unsigned concurrency_;
    std::vector<entry> ve_;
    any_fixed_array vw_;
    std::vector<std::size_t> vi_;
    std::size_t idle_ = 1;

    void(ports::*do_idle_)(void*);
    void(ports::*do_accept_)(std::size_t, entry&);
    void(ports::*do_stop_)();
};

} // http_io
} // boost

#endif
