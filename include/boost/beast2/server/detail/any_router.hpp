//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_DETAIL_ANY_ROUTER_HPP
#define BOOST_BEAST2_SERVER_DETAIL_ANY_ROUTER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/detail/call_traits.hpp>
#include <boost/beast2/detail/type_traits.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

struct Request;
struct Response;
//template<class Response, class Request>
//class basic_router;
namespace detail {
class any_router;
} // detail

//------------------------------------------------

namespace detail {

template<class T, class = void>
struct is_handler : std::false_type {};

template<class T>
struct is_handler<T> :
    detail::is_invocable<T,
    system::error_code, Request&, Response&>
{
};

template<class T, class = void>
struct is_error_handler : std::false_type {};

template<class T>
struct is_error_handler<T> :
    detail::is_invocable<T,
    system::error_code, Request&, Response&,
    system::error_code const>
{
};

template<class T, class = void>
struct is_router : std::false_type {};

template<class T>
struct is_router<T> :
    detail::derived_from<any_router, T>
{
};

} // detail

//------------------------------------------------

struct route_state;

//------------------------------------------------

namespace detail {

class any_router
{
protected:
    struct entry;
    struct impl;
    struct any_handler;
    struct any_errfn;
    struct req_info
    {
        http_proto::method method;
        std::string* base_path;
        std::string* suffix_path;
        urls::segments_encoded_view* path;
    };

    using handler_ptr = std::unique_ptr<any_handler>;
    using errfn_ptr = std::unique_ptr<any_errfn>;

    template<class Request, class Response,
        class Handler, class = void>
    struct handler_impl;

    template<class Request, class Response, class Handler>
    struct handler_impl<Request, Response, Handler,
        typename std::enable_if<detail::is_router<
            typename std::decay<Handler>::type>::value>::type>;

    // wrapper for error handling functions
    template<class Request, class Response, class Handler>
    struct errfn_impl;

    any_router() = default;

    BOOST_BEAST2_DECL ~any_router();
    BOOST_BEAST2_DECL any_router(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router(any_router const&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router const&) noexcept;

    BOOST_BEAST2_DECL any_router(req_info(*)(void*));
    BOOST_BEAST2_DECL std::size_t size() const noexcept;
    BOOST_BEAST2_DECL system::error_code invoke(
        void*, void*, route_state&) const;
    BOOST_BEAST2_DECL system::error_code resume(
        void*, void*, route_state&, system::error_code const& ec) const;

    BOOST_BEAST2_DECL void append(bool, http_proto::method,
        core::string_view, handler_ptr);
    BOOST_BEAST2_DECL void append_err(errfn_ptr);
    void append(bool, http_proto::method, core::string_view) {}

    impl* impl_ = nullptr;
};

//------------------------------------------------

struct BOOST_SYMBOL_VISIBLE
    any_router::any_handler
{
    // total children including this one
    std::size_t n_routes;

    BOOST_BEAST2_DECL
    virtual ~any_handler();

    virtual system::error_code operator()(
        void*, void*, route_state&) const = 0;
};

struct BOOST_SYMBOL_VISIBLE
    any_router::any_errfn
{
    BOOST_BEAST2_DECL
    virtual ~any_errfn();

    virtual system::error_code operator()(void* req,
        void* res, system::error_code const&) const = 0;
};

// wrapper for route handlers
template<
    class Request,
    class Response,
    class Handler,
    class>
struct any_router::handler_impl : any_handler
{
    typename std::decay<Handler>::type h;

    template<class... Args>
    handler_impl(Args&&... args)
        : h(std::forward<Args>(args)...)
    {
        n_routes = 1;
    }

    system::error_code operator()(
        void* req, void* res, route_state&) const override
    {
        return h(
            *reinterpret_cast<Request*>(req),
            *reinterpret_cast<Response*>(res));
    }
};

// wrapper for route handlers
template<
    class Request,
    class Response,
    class Handler>
struct any_router::handler_impl<Request, Response, Handler,
    typename std::enable_if<detail::is_router<
        typename std::decay<Handler>::type>::value>::type>
    : any_handler
{
    typename std::decay<Handler>::type h;

    template<class... Args>
    handler_impl(Args&&... args)
        : h(std::forward<Args>(args)...)
    {
        n_routes = h.size();
    }

    system::error_code operator()(
        void* req, void* res, route_state& st) const override
    {
        return h(*reinterpret_cast<Request*>(req),
            *reinterpret_cast<Response*>(res), st);
    }
};

// wrapper for error handling functions
template<class Request, class Response, class Handler>
struct any_router::errfn_impl : any_errfn
{
    typename std::decay<Handler>::type h;

    template<class... Args>
    errfn_impl(Args&&... args)
        : h(std::forward<Args>(args)...)
    {
    }

    system::error_code operator()(void* req, void* res,
        system::error_code const& ec) const override
    {
        return h(*reinterpret_cast<Request*>(req),
            *reinterpret_cast<Response*>(res), ec);
    }
};

} // detail
} // beast2
} // boost

#endif
