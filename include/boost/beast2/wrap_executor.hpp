//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_WRAP_EXECUTOR_HPP
#define BOOST_BEAST2_WRAP_EXECUTOR_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/capy/any_dispatcher.hpp>
#include <boost/asio/post.hpp>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast2 {

namespace detail {

/** A dispatcher that posts coroutine handles to an Asio executor.

    This class wraps an Asio executor and satisfies the capy::dispatcher
    concept. When invoked with a coroutine handle, it posts the handle's
    resumption to the wrapped executor for later execution.
*/
template<class AsioExecutor>
class asio_dispatcher
{
    AsioExecutor exec_;

public:
    explicit
    asio_dispatcher(AsioExecutor ex)
        : exec_(std::move(ex))
    {
    }

    asio_dispatcher(asio_dispatcher const&) = default;
    asio_dispatcher(asio_dispatcher&&) = default;
    asio_dispatcher& operator=(asio_dispatcher const&) = default;
    asio_dispatcher& operator=(asio_dispatcher&&) = default;

    /** Dispatch a coroutine handle for execution.

        Posts the coroutine's resumption to the Asio executor.
        Returns noop_coroutine since the work is scheduled for later.

        @param h The coroutine handle to dispatch.
        @return std::noop_coroutine() indicating no symmetric transfer.
    */
    capy::coro
    operator()(capy::coro h) const
    {
        asio::post(exec_, [h]() mutable {
            h.resume();
        });
        return std::noop_coroutine();
    }
};

} // detail

/** Return a capy::any_dispatcher from an Asio executor.

    This function wraps an Asio executor in a dispatcher that can be
    stored in capy::any_dispatcher. When the dispatcher is invoked with
    a coroutine handle, it posts the handle's resumption to the Asio
    executor for later execution.

    @param ex The Asio executor to wrap.

    @return A dispatcher that posts work to the provided Asio executor.
*/
template<class AsioExecutor>
detail::asio_dispatcher<typename std::decay<AsioExecutor>::type>
wrap_executor(AsioExecutor&& ex)
{
    return detail::asio_dispatcher<typename std::decay<AsioExecutor>::type>(
        std::forward<AsioExecutor>(ex));
}

} // beast2
} // boost

#endif
