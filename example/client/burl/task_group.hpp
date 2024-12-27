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

enum class task_group_errc
{
    cancelled = 1,
    closed
};

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
    close();

    void
    emit(asio::cancellation_type type);

    template<typename T, typename CompletionToken = asio::deferred_t>
    auto
    async_adapt(T&& t, CompletionToken&& completion_token = {});

    template<typename CompletionToken = asio::deferred_t>
    auto
    async_join(CompletionToken&& completion_token = {});
};

namespace boost
{
namespace system
{
template<>
struct is_error_code_enum<task_group_errc> : std::true_type
{
};
} // namespace system
} // namespace boost

inline const boost::system::error_category&
task_group_category()
{
    static const struct : boost::system::error_category
    {
        const char*
        name() const noexcept override
        {
            return "task_group";
        }

        std::string
        message(int ev) const override
        {
            switch(static_cast<task_group_errc>(ev))
            {
            case task_group_errc::cancelled:
                return "task_group cancelled";
            case task_group_errc::closed:
                return "task_group closed";
            default:
                return "Unknown task_group error";
            }
        }
    } category;

    return category;
};

inline std::error_code
make_error_code(task_group_errc e)
{
    return { static_cast<int>(e), task_group_category() };
}

template<typename Executor>
basic_task_group<Executor>::basic_task_group(Executor exec, std::uint32_t max)
    : cv_{ std::move(exec), asio::steady_timer::time_point::max() }
    , max_{ max }
{
}

template<typename Executor>
void
basic_task_group<Executor>::close()
{
    max_ = 0;
    cv_.cancel();
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

                if(ec == asio::error::operation_aborted)
                    ec.clear();

                if(!!self.cancelled())
                    ec = task_group_errc::cancelled;

                if(max_ == 0)
                    ec = task_group_errc::closed;

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
    return asio::async_compose<CompletionToken, void()>(
        [this, scheduled = false](auto&& self, error_code = {}) mutable
        {
            if(!scheduled)
                self.reset_cancellation_state(
                    asio::enable_total_cancellation());

            if(!!self.cancelled())
            {
                emit(self.cancelled());
                self.get_cancellation_state().clear();
            }

            if(!css_.empty())
            {
                scheduled = true;
                return cv_.async_wait(std::move(self));
            }

            if(!std::exchange(scheduled, true))
                return asio::async_immediate(
                    cv_.get_executor(), std::move(self));

            self.complete();
        },
        completion_token,
        cv_);
}

using task_group = basic_task_group<asio::any_io_executor>;

#endif
