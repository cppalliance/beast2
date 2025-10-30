//
// Copyright (c) 2025 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/error.hpp>
#include <boost/assert.hpp>
#include <cstring>

namespace boost {
namespace beast2 {

namespace detail {

const char*
error_cat_type::
name() const noexcept
{
    return "boost.beast2";
}

std::string
error_cat_type::
message(int code) const
{
    return message(code, nullptr, 0);
}

char const*
error_cat_type::
message(
    int code,
    char*,
    std::size_t) const noexcept
{
    switch(static_cast<error>(code))
    {
    case error::next: return "next";
    case error::detach: return "detach";
    case error::close: return "close";
    default:
        return "unknown";
    }
}

//-----------------------------------------------

// msvc 14.0 has a bug that warns about inability
// to use constexpr construction here, even though
// there's no constexpr construction
#if defined(_MSC_VER) && _MSC_VER <= 1900
# pragma warning( push )
# pragma warning( disable : 4592 )
#endif

#if defined(__cpp_constinit) && __cpp_constinit >= 201907L
constinit error_cat_type error_cat;
#else
error_cat_type error_cat;
#endif

#if defined(_MSC_VER) && _MSC_VER <= 1900
# pragma warning( pop )
#endif

} // detail

} // beast2
} // boost
