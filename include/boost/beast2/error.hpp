//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_ERROR_HPP
#define BOOST_BEAST2_ERROR_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/error_category.hpp>
#include <boost/system/is_error_code_enum.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

/** Error codes
*/
enum class error
{
    success = 0,
};

} // beast2

namespace system {
template<>
struct is_error_code_enum<
    ::boost::beast2::error>
{
    static bool const value = true;
};
} // system

namespace beast2 {

namespace detail {
struct BOOST_SYMBOL_VISIBLE
    error_cat_type
    : system::error_category
{
    BOOST_BEAST2_DECL const char* name(
        ) const noexcept override;
    BOOST_BEAST2_DECL std::string message(
        int) const override;
    BOOST_BEAST2_DECL char const* message(
        int, char*, std::size_t
            ) const noexcept override;
    BOOST_SYSTEM_CONSTEXPR error_cat_type()
        : error_category(0x515eb9dbd1314d96 )
    {
    }
};
BOOST_BEAST2_DECL extern error_cat_type error_cat;
} // detail

inline
BOOST_SYSTEM_CONSTEXPR
system::error_code
make_error_code(
    error ev) noexcept
{
    return system::error_code{
        static_cast<std::underlying_type<
            error>::type>(ev),
        detail::error_cat};
}

} // beast2
} // boost

#endif
