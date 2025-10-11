//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_FORMAT_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_FORMAT_HPP

//
// Format string support utility for server example.
//
// This header provides conditional compilation support for modern C++ formatting:
// - Uses std::format (via std::vformat) when C++20 is available
// - Falls back to fmtlib if <fmt/core.h> is available
// - Provides no formatting if neither is available (uses legacy stringstream)
//
// This allows the server example to benefit from format string support
// when available without requiring it as a hard dependency.
//

#include <string>

// Check for C++20 std::format support
#if __cplusplus >= 202002L && __has_include(<format>)
    #include <format>
    #define BOOST_HTTP_IO_HAS_STD_FORMAT
#elif __has_include(<fmt/core.h>)
    #include <fmt/core.h>
    #define BOOST_HTTP_IO_HAS_FMT
#endif

#if defined(BOOST_HTTP_IO_HAS_STD_FORMAT) || defined(BOOST_HTTP_IO_HAS_FMT)

namespace detail {

#if defined(BOOST_HTTP_IO_HAS_STD_FORMAT)
    template<typename... Args>
    inline std::string format(std::string_view fmt, Args&&... args)
    {
        return std::vformat(fmt, std::make_format_args(args...));
    }
#elif defined(BOOST_HTTP_IO_HAS_FMT)
    template<typename... Args>
    inline std::string format(std::string_view fmt, Args&&... args)
    {
        return fmt::format(fmt, std::forward<Args>(args)...);
    }
#endif

} // detail

#endif // format support available

#endif
