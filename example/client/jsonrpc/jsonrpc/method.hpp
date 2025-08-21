//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef JSONRPC_METHOD_HPP
#define JSONRPC_METHOD_HPP

#include <boost/core/detail/string_view.hpp>

namespace jsonrpc {

template<typename Signature>
struct method;

template<typename Return>
struct method<Return()>
{
    boost::core::string_view name;

    template<std::size_t N>
    constexpr method(const char (&n)[N])
        : name(n, N - 1)
    {
    }
};

template<typename Return, typename Param>
struct method<Return(Param)>
{
    boost::core::string_view name;

    template<std::size_t N>
    constexpr method(const char (&n)[N])
        : name(n, N - 1)
    {
    }
};

} // jsonrpc

#endif
