//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SPAWN_HPP
#define BOOST_BEAST2_SPAWN_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/capy/task.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/post.hpp>
#include <variant>

namespace boost {
namespace beast2 {

/** Launch a capy::task on the given executor.

    This function is similar to boost::asio::co_spawn, but is used to
    launch a capy::task instead of an asio::awaitable.
    @param ex The executor on which the task will be launched.
    @param t The task to launch.
    @param handler The completion handler to be invoked when the task
    completes. The handler signature is:
    @code
    void(std::variant<std::exception_ptr, T>)
    @endcode
    where the variant holds either an exception_ptr if an exception
    was thrown, or the result of type T.
    @return The result of the asynchronous initiation.
*/
template<
    class Executor,
    class T,
    class CompletionHandler>
auto spawn(
    Executor const& ex,
    capy::task<T> t,
    CompletionHandler&& handler)
{
    return asio::async_initiate<
        CompletionHandler,
        void(std::variant<std::exception_ptr, T>)>(
        [ex_ = ex](auto handler, capy::task<T> t)
        {
            auto h = t.release();
            auto* p = &h.promise();

            p->on_done_ = [handler = std::move(handler), h, p]() mutable
            {
                auto& r = p->result_;
                if (r.index() == 2)
                    std::move(handler)(std::variant<std::exception_ptr, T>(
                        std::in_place_index<0>, std::get<2>(r)));
                else
                    std::move(handler)(std::variant<std::exception_ptr, T>(
                        std::in_place_index<1>, std::move(std::get<1>(r))));
                h.destroy();
            };

            asio::post(ex_, [h]{ h.resume(); });
        },
        handler,
        std::move(t));
}

} // beast2
} // boost

#endif
