//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef JSONRPC_DETAIL_INITIATION_BASE_HPP
#define JSONRPC_DETAIL_INITIATION_BASE_HPP

#include <boost/asio/any_io_executor.hpp>

namespace jsonrpc {
namespace detail {

struct initiation_base
{
    boost::asio::any_io_executor ex;

    initiation_base(boost::asio::any_io_executor ex) noexcept
        : ex(std::move(ex))
    {
    }

    using executor_type = boost::asio::any_io_executor;
    const executor_type&
    get_executor() const noexcept
    {
        return ex;
    }
};

} // detail
} // jsonrpc

#endif
