//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef JSONRPC_DETAIL_CONVERTS_TO_HPP
#define JSONRPC_DETAIL_CONVERTS_TO_HPP

#include <boost/json/value.hpp>

namespace jsonrpc {
namespace detail {

template<typename T>
T*
converts_to(boost::json::value&)
{
    return nullptr;
}

template<>
inline
boost::json::string*
converts_to<boost::json::string>(boost::json::value& v)
{
    return v.if_string();
}

template<>
inline
boost::json::array*
converts_to<boost::json::array>(boost::json::value& v)
{
    return v.if_array();
}

template<>
inline
boost::json::object*
converts_to<boost::json::object>(boost::json::value& v)
{
    return v.if_object();
}

} // detail
} // jsonrpc

#endif
