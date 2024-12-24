//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_TASK_GROUP_HPP
#define BURL_TASK_GROUP_HPP

#include <boost/asio/append.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>

#include <list>

namespace asio   = boost::asio;
using error_code = boost::system::error_code;

class task_group
{
    asio::steady_timer cv_;
    std::uint32_t max_;
    std::list<asio::cancellation_signal> css_;

public:
    task_group(asio::any_io_executor exec, std::uint32_t max);

    task_group(task_group const&) = delete;
    task_group(task_group&&)      = delete;

    void
    emit(asio::cancellation_type type);

    bool
    empty() const;

    template<typename T, typename CompletionToken = asio::deferred_t>
    auto
    async_adapt(T&& t, CompletionToken&& completion_token = {});

    template<typename CompletionToken = asio::deferred_t>
    auto
    async_join(CompletionToken&& completion_token = {});

private:
    template<typename CompletionToken = asio::deferred_t>
    auto
    async_wait_for(std::size_t n, CompletionToken&& completion_token = {});
};

inline task_group::task_group(asio::any_io_executor exec, std::uint32_t max)
    : cv_{ std::move(exec), asio::steady_timer::time_point::max() }
    , max_{ max }
{
}

inline void
task_group::emit(asio::cancellation_type type)
{
    for(auto& cs : css_)
        cs.emit(type);
}

inline bool
task_group::empty() const
{
    return css_.empty();
}

template<typename CompletionToken>
auto
task_group::async_wait_for(std::size_t n, CompletionToken&& completion_token)
{
    return asio::async_compose<CompletionToken, void(error_code)>(
        [this, n, scheduled = false](auto&& self, error_code ec = {}) mutable
        {
            if(!scheduled)
                self.reset_cancellation_state(
                    asio::enable_total_cancellation());

            if(!self.cancelled() && ec == asio::error::operation_aborted)
                ec = {};

            if(css_.size() >= n && !ec)
            {
                scheduled = true;
                return cv_.async_wait(std::move(self));
            }

            if(!std::exchange(scheduled, true))
                return asio::post(std::move(self));

            self.complete(ec);
        },
        completion_token,
        cv_);
}

template<typename T, typename CompletionToken>
auto
task_group::async_adapt(T&& t, CompletionToken&& completion_token)
{
    class remover
    {
        task_group* tg_;
        decltype(css_)::iterator cs_;

    public:
        remover(task_group* tg, decltype(css_)::iterator cs)
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

    return async_wait_for(
        max_,
        asio::deferred(
            [this, t = std::move(t)](auto ec) mutable
            {
                auto cs = css_.emplace(css_.end());
                return asio::deferred.values(
                    ec,
                    asio::bind_cancellation_slot(
                        cs->slot(),
                        asio::consign(std::move(t), remover{ this, cs })));
            }))(std::forward<CompletionToken>(completion_token));
}

template<typename CompletionToken>
auto
task_group::async_join(CompletionToken&& completion_token)
{
    return async_wait_for(1, std::forward<CompletionToken>(completion_token));
}

#endif
