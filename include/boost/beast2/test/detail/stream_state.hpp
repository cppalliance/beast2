//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppaliance/beast2
//

#ifndef BOOST_BEAST2_TEST_DETAIL_STREAM_STATE_HPP
#define BOOST_BEAST2_TEST_DETAIL_STREAM_STATE_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/buffers/string_buffer.hpp>
#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/test/detail/service_base.hpp>
#include <boost/beast2/test/error.hpp>
#include <boost/beast2/test/fail_count.hpp>
#include <boost/make_shared.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace boost {
namespace beast2 {
namespace test {
namespace detail {

struct stream_state;

struct stream_service_impl
{
    std::mutex m_;
    std::vector<stream_state*> v_;

    void
    remove(stream_state& impl);
};

//------------------------------------------------------------------------------

class stream_service
    : public beast2::test::detail::service_base<stream_service>
{
    boost::shared_ptr<detail::stream_service_impl> sp_;

    void
    shutdown() override;

public:
    explicit
    stream_service(asio::execution_context& ctx);

    static
    auto
    make_impl(
        asio::any_io_executor exec,
        test::fail_count* fc) ->
            boost::shared_ptr<detail::stream_state>;
};

//------------------------------------------------------------------------------

struct stream_read_op_base
{
    virtual ~stream_read_op_base() = default;
    virtual void operator()(system::error_code ec) = 0;
};

//------------------------------------------------------------------------------

enum class stream_status
{
    ok,
    eof,
};

//------------------------------------------------------------------------------

struct stream_state
{
    asio::any_io_executor exec;
    boost::weak_ptr<stream_service_impl> wp;
    std::mutex m;
    std::string storage;
    buffers::string_buffer b;
    std::condition_variable cv;
    std::unique_ptr<stream_read_op_base> op;
    stream_status code = stream_status::ok;
    fail_count* fc = nullptr;
    std::size_t nread = 0;
    std::size_t nread_bytes = 0;
    std::size_t nwrite = 0;
    std::size_t nwrite_bytes = 0;
    std::size_t read_max =
        (std::numeric_limits<std::size_t>::max)();
    std::size_t write_max =
        (std::numeric_limits<std::size_t>::max)();

    stream_state(
        asio::any_io_executor exec_,
        boost::weak_ptr<stream_service_impl> wp_,
        fail_count* fc_);

    stream_state(stream_state&&) = delete;

    ~stream_state();

    void
    remove() noexcept;

    void
    notify_read();
};

//------------------------------------------------------------------------------

inline
stream_service::
stream_service(asio::execution_context& ctx)
    : beast2::test::detail::service_base<stream_service>(ctx)
    , sp_(boost::make_shared<stream_service_impl>())
{
}

inline
void
stream_service::
shutdown()
{
    std::vector<std::unique_ptr<detail::stream_read_op_base>> v;
    std::lock_guard<std::mutex> g1(sp_->m_);
    v.reserve(sp_->v_.size());
    for(auto p : sp_->v_)
    {
        std::lock_guard<std::mutex> g2(p->m);
        v.emplace_back(std::move(p->op));
        p->code = detail::stream_status::eof;
    }
}

inline
auto
stream_service::
make_impl(
    asio::any_io_executor exec,
    test::fail_count* fc) ->
    boost::shared_ptr<detail::stream_state>
{
#if defined(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)
    auto& ctx = exec.context();
#else
    auto& ctx = asio::query(
        exec,
        asio::execution::context);
#endif
    auto& svc = asio::use_service<stream_service>(ctx);
    auto sp = boost::make_shared<detail::stream_state>(exec, svc.sp_, fc);
    std::lock_guard<std::mutex> g(svc.sp_->m_);
    svc.sp_->v_.push_back(sp.get());
    return sp;
}

//------------------------------------------------------------------------------

inline
void
stream_service_impl::
remove(stream_state& impl)
{
    std::lock_guard<std::mutex> g(m_);
    *std::find(
        v_.begin(), v_.end(),
            &impl) = std::move(v_.back());
    v_.pop_back();
}

//------------------------------------------------------------------------------

inline
stream_state::
stream_state(
    asio::any_io_executor exec_,
    boost::weak_ptr<stream_service_impl> wp_,
    fail_count* fc_)
    : exec(std::move(exec_))
    , wp(std::move(wp_))
    , b(&storage)
    , fc(fc_)
{
}

inline
stream_state::
~stream_state()
{
    // cancel outstanding read
    if(op != nullptr)
        (*op)(asio::error::operation_aborted);
}

inline
void
stream_state::
remove() noexcept
{
    auto sp = wp.lock();

    // If this goes off, it means the lifetime of a test::stream object
    // extended beyond the lifetime of the associated execution context.
    BOOST_ASSERT(sp);

    sp->remove(*this);
}

inline
void
stream_state::
notify_read()
{
    if(op)
    {
        auto op_ = std::move(op);
        op_->operator()(system::error_code{});
    }
    else
    {
        cv.notify_all();
    }
}
} // detail
} // test
} // beast2
} // boost

#endif
