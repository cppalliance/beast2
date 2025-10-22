//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppaliance/beast2
//

#ifndef BOOST_BEAST2_TEST_DETAIL_SERVICE_BASE_HPP
#define BOOST_BEAST2_TEST_DETAIL_SERVICE_BASE_HPP

#include <boost/asio/execution_context.hpp>

namespace boost {
namespace beast2 {
namespace test {
namespace detail {

template<class T>
struct service_base : asio::execution_context::service
{
    static asio::execution_context::id const id;

    explicit
    service_base(asio::execution_context& ctx)
        : asio::execution_context::service(ctx)
    {
    }
};

template<class T>
asio::execution_context::id const service_base<T>::id;

} // detail
} // test
} // beast2
} // boost

#endif
