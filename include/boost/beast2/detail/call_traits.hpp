//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_DETAIL_CALL_TRAITS_HPP
#define BOOST_BEAST2_DETAIL_CALL_TRAITS_HPP

namespace boost {
namespace beast2 {
namespace detail {

template<class... Ts> struct type_list {};

template<class T> struct call_traits;

template<class R, class... Args>
struct call_traits<R(*)(Args...)>
{
    using return_type = R;
    using arg_types = type_list<Args...>;
};

template<class R, class... Args>
struct call_traits<R(&)(Args...)>
{
    using return_type = R;
    using arg_types = type_list<Args...>;
};

template<class C, class R, class... Args>
struct call_traits<R(C::*)(Args...)>
{
    using class_type = C;
    using return_type = R ;
    using arg_types = type_list<Args...>;
};

template<class C, class R, class... Args>
struct call_traits<R(C::*)(Args...) const>
{
    using class_type = C;
    using return_type = R ;
    using arg_types = type_list<Args...>;
};

template<class F>
struct call_traits
    : call_traits<decltype(&F::operator())>
{
};

} // detail
} // beast2
} // boost

#endif
