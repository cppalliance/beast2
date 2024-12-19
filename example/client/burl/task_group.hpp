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
    std::uint32_t max_;
    asio::steady_timer cv_;
    std::list<asio::cancellation_signal> css_;

public:
    task_group(asio::any_io_executor exec, std::uint32_t max)
        : cv_{ std::move(exec), asio::steady_timer::time_point::max() }
        , max_{ max }
    {
    }

    task_group(task_group const&) = delete;
    task_group(task_group&&)      = delete;

    template<
        typename T,
        typename CompletionToken =
            asio::default_completion_token_t<asio::any_io_executor>>
    auto
    async_adapt(T&& t, CompletionToken&& completion_token = {})
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

        auto adaptor = [&, t = std::move(t)]() mutable
        {
            auto cs = css_.emplace(css_.end());

            return asio::bind_cancellation_slot(
                cs->slot(), asio::consign(std::move(t), remover{ this, cs }));
        };

        return asio::async_compose<
            CompletionToken,
            void(boost::system::error_code, decltype(adaptor()))>(
            [this, scheduled = false, adaptor = std::move(adaptor)](
                auto&& self, boost::system::error_code ec = {}) mutable
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
                    return asio::post(asio::append(std::move(self), ec));

                self.complete(ec, adaptor());
            },
            completion_token,
            cv_);
    }

    void
    emit(asio::cancellation_type type)
    {
        for(auto& cs : css_)
            cs.emit(type);
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<asio::any_io_executor>>
    auto
    async_wait(CompletionToken&& completion_token = {})
    {
        return asio::
            async_compose<CompletionToken, void(boost::system::error_code)>(
                [this, scheduled = false](
                    auto&& self, boost::system::error_code ec = {}) mutable
                {
                    if(!scheduled)
                        self.reset_cancellation_state(
                            asio::enable_total_cancellation());

                    if(!self.cancelled() &&
                       ec == asio::error::operation_aborted)
                        ec = {};

                    if(!css_.empty() && !ec)
                    {
                        scheduled = true;
                        return cv_.async_wait(std::move(self));
                    }

                    if(!std::exchange(scheduled, true))
                        return asio::post(asio::append(std::move(self), ec));

                    self.complete(ec);
                },
                completion_token,
                cv_);
    }
};

#endif
