//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_DETAIL_GET_KEY_TYPE_HPP
#define BOOST_BEAST2_DETAIL_GET_KEY_TYPE_HPP

namespace boost {
namespace beast2 {
namespace detail {

template<class T, class = void>
struct get_key_type_impl
{
    using type = T;
};

template<class T>
struct get_key_type_impl<T,
    decltype(void(typename T::key_type()))>
{
    using type = typename T::key_type;
};

// Alias for T::key_type if it exists, otherwise T
template<class T>
using get_key_type =
    typename get_key_type_impl<T>::type;

} // detail
} // beast2
} // boost

#endif
