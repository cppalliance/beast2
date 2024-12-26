//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_basic_task_group_HPP
#define BURL_basic_task_group_HPP

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/steady_timer.hpp>

#include <list>

namespace asio   = boost::asio;
using error_code = boost::system::error_code;

template<typename Executor>
class basic_task_group
{
    asio::steady_timer::rebind_executor<Executor>::other cv_;
    std::uint32_t max_;
    std::list<asio::cancellation_signal> css_;

public:
    typedef Executor executor_type;

    template<typename Executor1>
    struct rebind_executor
    {
        typedef basic_task_group<Executor1> other;
    };

    basic_task_group(Executor exec, std::uint32_t max);

    basic_task_group(basic_task_group const&) = delete;
    basic_task_group(basic_task_group&&)      = delete;

    void
    emit(asio::cancellation_type type);

    template<typename T, typename CompletionToken = asio::deferred_t>
    auto
    async_adapt(T&& t, CompletionToken&& completion_token = {});

    template<typename CompletionToken = asio::deferred_t>
    auto
    async_join(CompletionToken&& completion_token = {});
};

template<typename Executor>
basic_task_group<Executor>::basic_task_group(
    Executor exec,
    std::uint32_t max)
    : cv_{ std::move(exec), asio::steady_timer::time_point::max() }
    , max_{ max }
{
}

template<typename Executor>
void
basic_task_group<Executor>::emit(asio::cancellation_type type)
{
    for(auto& cs : css_)
        cs.emit(type);
}

template<typename Executor>
template<typename T, typename CompletionToken>
auto
basic_task_group<Executor>::async_adapt(
    T&& t,
    CompletionToken&& completion_token)
{
    class remover
    {
        basic_task_group* tg_;
        decltype(css_)::iterator cs_;

    public:
        remover(basic_task_group* tg, decltype(css_)::iterator cs)
            : tg_{ tg }
            , cs_{ cs }
        {
        }

        remover(remover&& other) noexcept
            : tg_{ std::exchange(other.tg_, nullptr) }
            , cs_{ other.cs_ }
        {
        }

        ~remover()
        {
            if(tg_)
            {
                tg_->css_.erase(cs_);
                tg_->cv_.cancel();
            }
        }
    };

    auto adaptor = [this, t = std::move(t)]() mutable
    {
        auto cs = css_.emplace(css_.end());

        return asio::bind_cancellation_slot(
            cs->slot(), asio::consign(std::move(t), remover{ this, cs }));
    };

    return asio::
        async_compose<CompletionToken, void(error_code, decltype(adaptor()))>(
            [this, adaptor = std::move(adaptor), scheduled = false](
                auto&& self, error_code ec = {}) mutable
            {
                if(!scheduled)
                    self.reset_cancellation_state(
                        asio::enable_total_cancellation());

                if(!self.cancelled() && ec == asio::error::operation_aborted)
                    ec = {};

                if(css_.size() >= max_ && !ec)
                {
                    scheduled = true;
                    return cv_.async_wait(std::move(self));
                }

                if(!std::exchange(scheduled, true))
                    return asio::async_immediate(
                        cv_.get_executor(), std::move(self));

                self.complete(ec, adaptor());
            },
            completion_token,
            cv_);
}

template<typename Executor>
template<typename CompletionToken>
auto
basic_task_group<Executor>::async_join(CompletionToken&& completion_token)
{
    return asio::async_compose<CompletionToken, void(error_code)>(
        [this, scheduled = false](auto&& self, error_code ec = {}) mutable
        {
            if(!scheduled)
                self.reset_cancellation_state(
                    asio::enable_total_cancellation());

            if(ec == asio::error::operation_aborted)
                ec.clear();

            if(!!self.cancelled())
            {
                emit(self.cancelled());
                self.get_cancellation_state().clear();
            }

            if(!css_.empty() && !ec)
            {
                scheduled = true;
                return cv_.async_wait(std::move(self));
            }

            if(!std::exchange(scheduled, true))
                return asio::async_immediate(
                    cv_.get_executor(), std::move(self));

            self.complete(ec);
        },
        completion_token,
        cv_);
}

using task_group = basic_task_group<asio::any_io_executor>;

#endif
