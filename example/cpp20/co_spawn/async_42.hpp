//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/capy
//

#ifndef BOOST_BEAST2_TASK_HPP
#define BOOST_BEAST2_TASK_HPP

#include <boost/asio/async_result.hpp>
#include <boost/asio/post.hpp>

namespace boost {
namespace beast2 {

template<class Executor, class CompletionToken>
auto async_42(Executor const& exec, CompletionToken&& token)
{
    return asio::async_initiate<CompletionToken, void(int)>(
        [](auto handler, Executor exec)
        {
            boost::asio::post(exec,
                [handler = std::move(handler)]() mutable
                {
                    std::move(handler)(42);
                });
        },
        token,
        exec);
}

} // beast2
} // boost

#endif
