//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppaliance/beast2
//

#ifndef BOOST_BEAST2_TEST_IMPL_STREAM_HPP
#define BOOST_BEAST2_TEST_IMPL_STREAM_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/test/detail/service_base.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/buffers/copy.hpp>

#include <mutex>

namespace boost {
namespace beast2 {
namespace test {

namespace detail
{
template<class To>
struct extract_executor_op
{
    To operator()(asio::any_io_executor& ex) const
    {
        assert(ex.template target<To>());
        return *ex.template target<To>();
    }
};

template<>
struct extract_executor_op<asio::any_io_executor>
{
    asio::any_io_executor operator()(asio::any_io_executor& ex) const
    {
        return ex;
    }
};

template<class Handler, class Buffers, bool Reader>
class lambda_base
{
protected:
    Handler h_;
    boost::weak_ptr<detail::stream_state> iwp_;
    boost::weak_ptr<detail::stream_state> owp_;
    Buffers b_;
    asio::executor_work_guard<
        asio::associated_executor_t<Handler, asio::any_io_executor>>
        wg2_;

    class cancellation_handler
    {
    public:
        explicit cancellation_handler(
            boost::weak_ptr<detail::stream_state> wp)
            : wp_(std::move(wp))
        {
        }

        void
        operator()(asio::cancellation_type type) const
        {
            if(type != asio::cancellation_type::none)
            {
                if(auto sp = wp_.lock())
                {
                    std::unique_ptr<stream_op_base> p;
                    {
                        std::lock_guard<std::mutex> lock(sp->m);
                        if(Reader)
                            p = std::move(sp->rop);
                        else
                            p = std::move(sp->wop);
                    }
                    if(p != nullptr)
                        (*p)(asio::error::operation_aborted);
                }
            }
        }

    private:
        boost::weak_ptr<detail::stream_state> wp_;
    };

public:
    template<class Handler_>
    lambda_base(
        Handler_&& h,
        boost::shared_ptr<detail::stream_state> const& in,
        boost::weak_ptr<detail::stream_state> const& out,
        Buffers const& b)
        : h_(std::forward<Handler_>(h))
        , iwp_(in)
        , owp_(out)
        , b_(b)
        , wg2_(asio::get_associated_executor(h_, in->exec))
    {
        auto c_slot = asio::get_associated_cancellation_slot(h_);
        if(c_slot.is_connected())
            c_slot.template emplace<cancellation_handler>(iwp_);
    }

    using allocator_type = asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return asio::get_associated_allocator(h_);
    }

    using cancellation_slot_type =
        asio::associated_cancellation_slot_t<Handler>;

    cancellation_slot_type
    get_cancellation_slot() const noexcept
    {
        return asio::get_associated_cancellation_slot(
            h_, asio::cancellation_slot());
    }
};

} // detail

template<class Executor>
template<class Handler, class Buffers>
class basic_stream<Executor>::read_op : public detail::stream_op_base
{
    class lambda : public detail::lambda_base<Handler, Buffers, true>
    {
    public:
        using base = detail::lambda_base<Handler, Buffers, true>;

        void
        operator()(system::error_code ec)
        {
            std::size_t bytes_transferred = 0;
            auto sp = base::iwp_.lock();
            if(! sp)
            {
                ec = asio::error::operation_aborted;
            }
            if(!ec)
            {
                std::lock_guard<std::mutex> lock(sp->m);
                BOOST_ASSERT(!sp->rop);
                if(sp->b.size() > 0)
                {
                    bytes_transferred = buffers::copy(
                        base::b_,
                        sp->b.data(),
                        sp->read_max);
                    sp->b.consume(bytes_transferred);
                    sp->nread_bytes += bytes_transferred;
                }
                else if(
                    buffers::size(
                        base::b_) > 0)
                {
                    ec = asio::error::eof;
                }
            }

            asio::dispatch(
                base::wg2_.get_executor(),
                asio::append(std::move(base::h_), ec, bytes_transferred));
            base::wg2_.reset();
            sp->rop.reset(nullptr);
        }

        template<class Handler_>
        lambda(
            Handler_&& h,
            boost::shared_ptr<detail::stream_state> const& in,
            boost::weak_ptr<detail::stream_state> const& out,
            Buffers const& b)
            : base(std::forward<Handler_>(h), in, out, b)
        {
        }
    };

    std::unique_ptr<lambda> fnp_;
    asio::executor_work_guard<asio::any_io_executor> wg1_;

public:
    template<class Handler_>
    read_op(
        Handler_&& h,
        boost::shared_ptr<detail::stream_state> const& in,
        boost::weak_ptr<detail::stream_state> const& out,
        Buffers const& b)
        : fnp_(new lambda(std::forward<Handler_>(h), in, out, b))
        , wg1_(in->exec)
    {
    }

    void
    operator()(system::error_code ec) override
    {
        std::unique_ptr<lambda> fnp(std::move(fnp_));
        if(fnp)
            asio::post(
                wg1_.get_executor(), asio::append(std::move(*fnp), ec));
        wg1_.reset();
    }
};

template<class Executor>
template<class Handler, class Buffers>
class basic_stream<Executor>::write_op : public detail::stream_op_base
{
    class lambda : public detail::lambda_base<Handler, Buffers, false>
    {
    public:
        using base = detail::lambda_base<Handler, Buffers, false>;

        void
        operator()(system::error_code ec)
        {
            std::size_t bytes_transferred = 0;
            auto isp = base::iwp_.lock();
            if(!isp)
            {
                ec = asio::error::operation_aborted;
            }
            auto osp = base::owp_.lock();
            if(!osp)
            {
                ec = asio::error::operation_aborted;
            }
            if(!ec)
            {
                // copy buffers
                std::size_t n = std::min<std::size_t>(
                    buffers::size(base::b_), isp->write_max);
                {
                    std::lock_guard<std::mutex> lock(osp->m);
                    n = buffers::copy(osp->b.prepare(n), base::b_);
                    osp->b.commit(n);
                    osp->nwrite_bytes += n;
                    osp->notify_read();
                }
                bytes_transferred = n;
            }

            asio::dispatch(
                base::wg2_.get_executor(),
                asio::append(std::move(base::h_), ec, bytes_transferred));
            base::wg2_.reset();
            isp->wop.reset(nullptr);
        }

        template<class Handler_>
        lambda(
            Handler_&& h,
            boost::shared_ptr<detail::stream_state> const& in,
            boost::weak_ptr<detail::stream_state> const& out,
            Buffers const& b)
            : base(std::forward<Handler_>(h), in, out, b)
        {
        }
    };

    std::unique_ptr<lambda> fnp_;
    asio::executor_work_guard<asio::any_io_executor> wg1_;

public:
    template<class Handler_>
    write_op(
        Handler_&& h,
        boost::shared_ptr<detail::stream_state> const& in,
        boost::weak_ptr<detail::stream_state> const& out,
        Buffers const& b)
        : fnp_(new lambda(std::forward<Handler_>(h), in, out, b))
        , wg1_(in->exec)
    {
    }

    void
    operator()(system::error_code ec) override
    {
        std::unique_ptr<lambda> fnp(std::move(fnp_));
        if(fnp)
            asio::post(wg1_.get_executor(), asio::append(std::move(*fnp), ec));
        wg1_.reset();
    }
};

template<class Executor>
struct basic_stream<Executor>::run_read_op
{
    boost::shared_ptr<detail::stream_state> const& in_;

    using executor_type = typename basic_stream::executor_type;

    executor_type
    get_executor() const noexcept
    {
        return detail::extract_executor_op<Executor>()(in_->exec);
    }

    template<
        class ReadHandler,
        class MutableBufferSequence>
    void
    operator()(
        ReadHandler&& h,
        boost::weak_ptr<detail::stream_state> out,
        MutableBufferSequence const& buffers)
    {
        // If you get an error on the following line it means
        // that your handler does not meet the documented type
        // requirements for the handler.

        initiate_read(
            in_,
            out,
            std::unique_ptr<detail::stream_op_base>{ new read_op<
                typename std::decay<ReadHandler>::type,
                MutableBufferSequence>(std::move(h), in_, out, buffers) },
            buffers::size(buffers));
    }
};

template<class Executor>
struct basic_stream<Executor>::run_write_op
{
    boost::shared_ptr<detail::stream_state> const& in_;

    using executor_type = typename basic_stream::executor_type;

    executor_type
    get_executor() const noexcept
    {
        return detail::extract_executor_op<Executor>()(in_->exec);
    }

    template<
        class WriteHandler,
        class ConstBufferSequence>
    void
    operator()(
        WriteHandler&& h,
        boost::weak_ptr<detail::stream_state> out,
        ConstBufferSequence const& buffers)
    {
        // If you get an error on the following line it means
        // that your handler does not meet the documented type
        // requirements for the handler.

        initiate_write(
            in_,
            out,
            std::unique_ptr<detail::stream_op_base>{ new write_op<
                typename std::decay<WriteHandler>::type,
                ConstBufferSequence>(std::move(h), in_, out, buffers) },
            buffers::size(buffers));
    }
};

//------------------------------------------------------------------------------

template<class Executor>
template<class MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, std::size_t)) ReadHandler>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void(system::error_code, std::size_t))
basic_stream<Executor>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(asio::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence type requirements not met");

    return asio::async_initiate<
        ReadHandler,
        void(system::error_code, std::size_t)>(
            run_read_op{in_},
            handler,
            out_,
            buffers);
}

template<class Executor>
template<class ConstBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(system::error_code, std::size_t)) WriteHandler>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WriteHandler, void(system::error_code, std::size_t))
basic_stream<Executor>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(asio::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence type requirements not met");

    return asio::async_initiate<
        WriteHandler,
        void(system::error_code, std::size_t)>(
            run_write_op{in_},
            handler,
            out_,
            buffers);
}

//------------------------------------------------------------------------------

template<class Executor, class Arg1, class... ArgN>
basic_stream<Executor>
connect(stream& to, Arg1&& arg1, ArgN&&... argn)
{
    stream from{
        std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...};
    from.connect(to);
    return from;
}

template<class Executor>
auto basic_stream<Executor>::get_executor() noexcept -> executor_type
{
    return detail::extract_executor_op<Executor>()(in_->exec);
}

//------------------------------------------------------------------------------

template<class Executor>
void
basic_stream<Executor>::initiate_read(
    boost::shared_ptr<detail::stream_state> const& in,
    boost::weak_ptr<detail::stream_state> const& out,
    std::unique_ptr<detail::stream_op_base>&& rop,
    std::size_t buf_size)
{
    (void)out;

    std::unique_lock<std::mutex> lock(in->m);

    ++in->nread;
    if(in->rop != nullptr)
        BOOST_THROW_EXCEPTION(std::logic_error{ "in_->rop != nullptr" });

    // test failure
    system::error_code ec;
    if(in->fc && in->fc->fail(ec))
    {
        lock.unlock();
        (*rop)(ec);
        return;
    }

    // A request to read 0 bytes from a stream is a no-op.
    if(buf_size == 0 || buffers::size(in->b.data()) > 0)
    {
        lock.unlock();
        (*rop)(ec);
        return;
    }

    // deliver error
    if(in->code != detail::stream_status::ok)
    {
        lock.unlock();
        (*rop)(asio::error::eof);
        return;
    }

    // complete when bytes available or closed
    in->rop = std::move(rop);
}

//------------------------------------------------------------------------------

template<class Executor>
void basic_stream<Executor>::initiate_write(
    boost::shared_ptr<detail::stream_state> const& in,
    boost::weak_ptr<detail::stream_state> const& out,
    std::unique_ptr<detail::stream_op_base>&& wop,
    std::size_t buf_size)
{
    {
        std::unique_lock<std::mutex> lock(in->m);

        ++in->nwrite;

        // test failure
        system::error_code ec;
        if(in->fc && in->fc->fail(ec))
        {
            lock.unlock();
            (*wop)(ec);
            return;
        }
    }

    // A request to write 0 bytes to a stream is a no-op.
    if(buf_size == 0)
    {
        (*wop)(system::error_code{});
        return;
    }

    // connection closed
    auto osp = out.lock();
    if(!osp)
    {
        (*wop)(asio::error::connection_reset);
        return;
    }

    in->wop = std::move(wop);
    //auto op = std::move(in_->wop);
    in->wop->operator()(system::error_code{});
}

//------------------------------------------------------------------------------

template<class Executor>
basic_stream<Executor>::
~basic_stream()
{
    close();
    in_->remove();
}

template<class Executor>
basic_stream<Executor>::
basic_stream(basic_stream&& other)
{
    auto in = detail::stream_service::make_impl(
        other.in_->exec, other.in_->fc);
    in_ = std::move(other.in_);
    out_ = std::move(other.out_);
    other.in_ = in;
}


template<class Executor>
basic_stream<Executor>&
basic_stream<Executor>::
operator=(basic_stream&& other)
{
    close();
    auto in = detail::stream_service::make_impl(
        other.in_->exec, other.in_->fc);
    in_->remove();
    in_ = std::move(other.in_);
    out_ = std::move(other.out_);
    other.in_ = in;
    return *this;
}

//------------------------------------------------------------------------------

template<class Executor>
basic_stream<Executor>::
basic_stream(executor_type exec)
    : in_(detail::stream_service::make_impl(std::move(exec), nullptr))
{
}

template<class Executor>
basic_stream<Executor>::
basic_stream(
    asio::io_context& ioc,
    fail_count& fc)
    : in_(detail::stream_service::make_impl(ioc.get_executor(), &fc))
{
}

template<class Executor>
basic_stream<Executor>::
basic_stream(
    asio::io_context& ioc,
    core::string_view s)
    : in_(detail::stream_service::make_impl(ioc.get_executor(), nullptr))
{
    in_->b.commit(buffers::copy(
        in_->b.prepare(s.size()),
        buffers::const_buffer(s.data(), s.size())));
}

template<class Executor>
basic_stream<Executor>::
basic_stream(
    asio::io_context& ioc,
    fail_count& fc,
    core::string_view s)
    : in_(detail::stream_service::make_impl(ioc.get_executor(), &fc))
{
    in_->b.commit(buffers::copy(
        in_->b.prepare(s.size()),
        buffers::const_buffer(s.data(), s.size())));
}

template<class Executor>
void
basic_stream<Executor>::
connect(basic_stream& remote)
{
    BOOST_ASSERT(! out_.lock());
    BOOST_ASSERT(! remote.out_.lock());
    std::lock(in_->m, remote.in_->m);
    std::lock_guard<std::mutex> guard1{in_->m, std::adopt_lock};
    std::lock_guard<std::mutex> guard2{remote.in_->m, std::adopt_lock};
    out_ = remote.in_;
    remote.out_ = in_;
    in_->code = detail::stream_status::ok;
    remote.in_->code = detail::stream_status::ok;
}

template<class Executor>
core::string_view
basic_stream<Executor>::
str() const
{
    auto const bs = in_->b.data();
    if(buffers::size(bs) == 0)
        return {};
    buffers::const_buffer const b = *asio::buffer_sequence_begin(bs);
    return {static_cast<char const*>(b.data()), b.size()};
}

template<class Executor>
void
basic_stream<Executor>::
append(core::string_view s)
{
    std::lock_guard<std::mutex> lock{in_->m};
    in_->b.commit(buffers::copy(
        in_->b.prepare(s.size()),
        buffers::const_buffer(s.data(), s.size())));
}

template<class Executor>
void
basic_stream<Executor>::
clear()
{
    std::lock_guard<std::mutex> lock{in_->m};
    in_->b.consume(in_->b.size());
}

template<class Executor>
void
basic_stream<Executor>::
close()
{
    {
        std::lock_guard<std::mutex> lock(in_->m);
        in_->code = detail::stream_status::eof;
        in_->notify_read();
    }

    // disconnect
    {
        auto out = out_.lock();
        out_.reset();

        // notify peer
        if(out)
        {
            std::lock_guard<std::mutex> lock(out->m);
            if(out->code == detail::stream_status::ok)
            {
                out->code = detail::stream_status::eof;
                out->notify_read();
            }
        }
    }
}

template<class Executor>
void
basic_stream<Executor>::
close_remote()
{
    std::lock_guard<std::mutex> lock{in_->m};
    if(in_->code == detail::stream_status::ok)
    {
        in_->code = detail::stream_status::eof;
        in_->notify_read();
    }
}

//------------------------------------------------------------------------------

template<class Executor>
basic_stream<Executor>
connect(basic_stream<Executor>& to)
{
    basic_stream<Executor> from(to.get_executor());
    from.connect(to);
    return from;
}

template<class Executor>
void
connect(basic_stream<Executor>& s1, basic_stream<Executor>& s2)
{
    s1.connect(s2);
}

} // test
} // beast2
} // boost

#endif
