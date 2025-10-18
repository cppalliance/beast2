//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_ROUTER_HPP
#define BOOST_HTTP_IO_SERVER_ROUTER_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/core/detail/string_view.hpp>
#include <memory>
#include <new>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace boost {
namespace http_io {

// Enum for the different token types in a parsed path pattern
enum class TokenType { Text, Param, Wildcard, Group };

template<class T>
class router;

template<class T, class... Args>
void emplace(
    router<T>&,
    http_proto::method,
    core::string_view,
    Args&&...);

namespace detail {
class router_base
{
public:
    struct BOOST_SYMBOL_VISIBLE
        handler
    {
        BOOST_HTTP_IO_DECL
        virtual ~handler();

        virtual void operator()(void*) const = 0;
    };

protected:
    struct entry;
    struct impl;

    BOOST_HTTP_IO_DECL
    router_base();

    BOOST_HTTP_IO_DECL
    void insert(
        http_proto::method method,
        core::string_view path,
        std::unique_ptr<handler>);

    BOOST_HTTP_IO_DECL
    void invoke_impl(
        http_proto::method method,
        core::string_view path,
        void* param) const;

    template<class T, class... Args>
    friend
    void http_io::emplace(
        router<T>& r,
        http_proto::method method,
        core::string_view path,
        Args&&... args);

    std::shared_ptr<impl> impl_;
    std::unique_ptr<handler> h_;
};
} // detail

/** A container mapping HTTP requests to handlers
*/
template<class T>
class router : private detail::router_base
{
public:
    struct route
    {
        span<std::pair<std::string const, TokenType const>> keys;
    };

    template<
        class Handler,
        class T_,
        class... Args>
    friend void
    emplace(
        router<T_>& r,
        http_proto::method method,
        core::string_view path,
        Args&&... args);

    /** Add a GET route
    */
    template<
        class Handler,
        class... Args>
    void
    get(
        core::string_view path,
        Args&&... args);

    bool
    invoke(
        http_proto::method method,
        core::string_view path,
        T&& param) const
    {
        invoke_impl(method, path, &param);
        return true;
    }
};

//------------------------------------------------

template<
    class Handler,
    class T,
    class... Args>
void
emplace(
    router<T>& r,
    http_proto::method method,
    core::string_view path,
    Args&&... args)
{
    (void)method;
    (void)path;

    struct impl : detail::router_base::handler
    {
        Handler h;

        impl(Args&&... args)
            : h(std::forward<Args>(args)...)
        {
        }

        void operator()(void* p) const override
        {
            h(*reinterpret_cast<T*>(p));
        }
    };

    r.insert(method, path, std::unique_ptr<impl>(new
        impl{std::forward<Args>(args)...}));
}

template<class T>
template<
    class Handler,
    class... Args>
void
router<T>::
get(
    core::string_view path,
    Args&&... args)
{
    emplace<Handler>(
        *this,
        http_proto::method::get,
        path,
        std::forward<Args>(args)...);
}

} // http_io
} // boost

#endif
