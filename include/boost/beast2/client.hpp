//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_CLIENT_HPP
#define BOOST_BEAST2_CLIENT_HPP

#include <boost/beast2/detail/config.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast2 {

template<
    class AsyncStream
    /*,class Derived*/ // VFALCO CRTP for things like shutdown()
>
class client
{
public:
    using stream_type = typename
        std::remove_reference<AsyncStream>::type;

    using executor_type = decltype(
        std::declval<stream_type&>().get_executor());

    template<
        class... Args,
        class = std::enable_if<
            std::is_constructible<
                AsyncStream, Args...>::value>
    >
    explicit
    client(
        Args&&... args) noexcept(
            std::is_nothrow_constructible<
                AsyncStream, Args...>::value)
        : stream_(std::forward<Args>(args)...)
    {
    }

    stream_type const&
    stream() const noexcept
    {
        return stream_;
    }

    stream_type&
    stream() noexcept
    {
        return stream_;
    }

    executor_type
    get_executor() const
    {
        return stream_.get_executor();
    }

private:
    AsyncStream stream_;
};

} // beast2
} // boost

#endif
