//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef JSONRPC_ERRC_HPP
#define JSONRPC_ERRC_HPP

#include <boost/system/error_code.hpp>
#include <boost/system/error_category.hpp>

namespace jsonrpc {

/// Error codes returned stream operations
enum class errc
{
    /// The response object is not JSON-RPC 2.0.
    version_error,

    /// The response object has a different ID than the request.
    id_mismatch,

    /// The HTTP body is not a valid response object.
    invalid_response,

    /// Invalid JSON was received by the server.
    parse_error,

    /// The JSON sent is not a valid Request object.
    invalid_request,

    /// The method does not exist.
    method_not_found,

    /// Invalid method parameter(s).
    invalid_params,

    /// Internal JSON-RPC error.
    internal_error,

    /// Server error, see error object for details.
    server_error
};

} // jsonrpc

namespace boost {
namespace system {
template<>
struct is_error_code_enum<jsonrpc::errc>
{
    static bool const value = true;
};
} // system
} // boost

namespace jsonrpc {
namespace detail {
struct BOOST_SYMBOL_VISIBLE
    error_cat_type
    : boost::system::error_category
{
    BOOST_SYSTEM_CONSTEXPR error_cat_type()
        : error_category(0x90c273d3719bc3f2)
    {                       
    }
    const char* name() const noexcept override;
    std::string message(int) const override;
    char const* message(
        int, char*, std::size_t) const noexcept override;
};

extern
error_cat_type error_cat;
} // detail

inline
BOOST_SYSTEM_CONSTEXPR
boost::system::error_code
make_error_code(errc ev) noexcept
{
    return boost::system::error_code{
        static_cast<std::underlying_type<
            errc>::type>(ev),
        detail::error_cat};
}

} // jsonrpc

#endif
