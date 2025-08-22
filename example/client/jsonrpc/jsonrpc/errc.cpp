//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "errc.hpp"

namespace jsonrpc {
namespace detail {

const char*
error_cat_type::
name() const noexcept
{
    return "jsonrpc";
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
    switch (static_cast<errc>(code))
    {
    case errc::version_error:
        return "The response object is not JSON-RPC version 2.0";
    case errc::id_mismatch:
        return "The response object has a different ID than the request";
    case errc::invalid_response:
        return "The received body does not contain a valid response object";
    case errc::parse_error:
        return "Invalid JSON was received by the server";
    case errc::invalid_request:
        return "The JSON sent is not a valid Request object";
    case errc::method_not_found:
        return "The method does not exist";
    case errc::invalid_params:
        return "Invalid method parameter(s)";
    case errc::internal_error:
        return "Internal JSON-RPC error";
    case errc::server_error:
        return "Server error";
    default:
        return "unknown";
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
constinit error_cat_type error_cat;
#else
error_cat_type error_cat;
#endif

#if defined(_MSC_VER) && _MSC_VER <= 1900
# pragma warning( pop )
#endif

} // detail
}
