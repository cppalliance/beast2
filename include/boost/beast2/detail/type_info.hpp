//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_DETAIL_TYPE_INFO_HPP
#define BOOST_BEAST2_DETAIL_TYPE_INFO_HPP

#include <cstdint>
#ifndef BOOST_NO_RTTI
#include <typeinfo>
#endif

namespace boost {
namespace beast2 {
namespace detail {

#ifdef BOOST_NO_RTTI

struct type_info
{
    std::size_t hash_code() const noexcept
    {
        return reinterpret_cast<std::uintptr_t>(this);
    }

    friend bool operator==(type_info const& lhs,
        type_info const& rhs) noexcept
    {
        return &lhs == &rhs;
    }

    friend bool operator!=(type_info const& lhs,
        type_info const& rhs) noexcept
    {
        return &lhs != &rhs;
    }
};

template<class T>
struct type_info_for
{
    static type_info const value;
};

template<class T>
type_info const type_info_for<T>::value{};

template<class T>
type_info const& get_type_info() noexcept
{
    return type_info_for<T>::value;
}

#else

using type_info = std::type_info;

template<class T>
type_info const& get_type_info() noexcept
{
    return typeid(T);
}

#endif

} // detail
} // beast2
} // boost

#endif
