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

#include <type_traits>

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

// Helper to check if T is callable with Args...
template<class T, class... Args>
class is_invocable_impl
{
    // Try calling T with Args...
    template<class U>
    static auto test(int) -> decltype(std::declval<U>()(
        std::declval<Args>()...), std::true_type{}
    );

    template<class>
    static auto test(...) -> std::false_type;

public:
    static const bool value = decltype(test<T>(0))::value;
};

// Primary template
template<class T, class R, class... Args>
struct is_invocable : std::integral_constant<bool,
    is_invocable_impl<T, Args...>::value &&
    std::is_convertible<typename call_traits<T>::return_type,R>::value>
{
};


} // detail
} // beast2
} // boost

#endif
