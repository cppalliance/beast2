//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef JSONRPC_ERROR_HPP
#define JSONRPC_ERROR_HPP

#include "errc.hpp"

#include <boost/asio/disposition.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>

namespace jsonrpc {

/** An error type that encapsulates both an error code
    and an optional JSON object containing additional error
    details as provided by the server.
*/
class error
{
    boost::system::error_code ec_;
    boost::json::object object_;

public:
    error(
        boost::system::error_code ec,
        boost::json::object object = {}) noexcept
        : ec_(ec)
        , object_(std::move(object))
    {
    }

    error() noexcept = default;
    error(error&&) noexcept = default;

    error&
    operator=(error&& other) noexcept
    {
        ec_ = std::move(other.ec_);
        object_ = std::move(other.object_);
        return *this;
    }

    /// Return the error code
    const boost::system::error_code&
    code() const noexcept
    {
        return ec_;
    }

    /** Return an optional JSON object containing additional
        error details as provided by the server.
    */
    const boost::json::object&
    object() const noexcept
    {
        return object_;
    }
};

} // jsonrpc

namespace boost {
namespace asio {
template<>
struct disposition_traits<jsonrpc::error>
{
    static bool
    not_an_error(const jsonrpc::error& e) noexcept
    {
        return !e.code();
    }

    static void
    throw_exception(const jsonrpc::error& e)
    {
        if(e.object().empty())
            return BOOST_THROW_EXCEPTION(system::system_error(e.code()));

        BOOST_THROW_EXCEPTION(
            system::system_error(e.code(), json::serialize(e.object())));
    }

    static std::exception_ptr
    to_exception_ptr(const jsonrpc::error& e) noexcept
    {
        if(!e.code())
            return nullptr;

        if(e.object().empty())
            return std::make_exception_ptr(system::system_error(e.code()));

        return std::make_exception_ptr(
            system::system_error(e.code(), json::serialize(e.object())));
    }
};
} // asio
} // boost

#endif
