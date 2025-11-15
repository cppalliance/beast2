//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/router_types.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/assert.hpp>
#include <cstring>

namespace boost {
namespace beast2 {

namespace detail {

const char*
route_cat_type::
name() const noexcept
{
    return "boost.http_proto.route";
}

std::string
route_cat_type::
message(int code) const
{
    return message(code, nullptr, 0);
}

char const*
route_cat_type::
message(
    int code,
    char*,
    std::size_t) const noexcept
{
    switch(static_cast<route>(code))
    {
    case route::send:       return "http_proto::route::send";
    case route::next:       return "http_proto::route::next";
    case route::next_route: return "http_proto::route::next_route";
    case route::close:      return "http_proto::route::close";
    case route::detach:     return "http_proto::route::detach";
    case route::complete:   return "http_proto::route::complete";
    default:
        return "http_proto::route::?";
    }
}

// msvc 14.0 has a bug that warns about inability
// to use constexpr construction here, even though
// there's no constexpr construction
#if defined(_MSC_VER) && _MSC_VER <= 1900
# pragma warning( push )
# pragma warning( disable : 4592 )
#endif

#if defined(__cpp_constinit) && __cpp_constinit >= 201907L
constinit route_cat_type route_cat;
#else
route_cat_type route_cat;
#endif

#if defined(_MSC_VER) && _MSC_VER <= 1900
# pragma warning( pop )
#endif

} // detail

} // beast2
} // boost
