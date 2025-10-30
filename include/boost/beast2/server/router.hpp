//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTER_HPP
#define BOOST_BEAST2_SERVER_ROUTER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/detail/call_traits.hpp>
#include <boost/beast2/detail/type_traits.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast2 {

struct Request;
struct Response;
class router_base;
template<class Response, class Request>
class router;

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
    system::error_code>
{
};

template<class T, class = void>
struct is_router : std::false_type {};

template<class T>
struct is_router<T> :
    detail::derived_from<router_base, T>
{
};

} // detail

//------------------------------------------------

struct route_state
{
private:
    friend class router_base;
    template<class Res, class Req>
    friend class router;

    std::size_t pos = 0;
    std::size_t resume = 0;
    system::error_code ec;
};

class router_base
{
protected:
    struct entry;
    struct impl;
    struct any_handler;
    struct any_errfn;

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

    BOOST_BEAST2_DECL router_base(
        http_proto::method(*)(void*),
        urls::segments_encoded_view&(*)(void*));
    BOOST_BEAST2_DECL std::size_t size() const noexcept;
    BOOST_BEAST2_DECL system::error_code invoke(
        void*, void*, route_state&) const;
    BOOST_BEAST2_DECL system::error_code resume(
        void*, void*, route_state&, system::error_code const& ec) const;

    BOOST_BEAST2_DECL void append(bool, http_proto::method,
        core::string_view, handler_ptr);
    BOOST_BEAST2_DECL void append_err(errfn_ptr);
    void append(bool, http_proto::method, core::string_view) {}

    std::shared_ptr<impl> impl_;
};

//------------------------------------------------

/** A container mapping HTTP requests to handlers
*/
template<class Request, class Response>
class router : public router_base
{
    static constexpr http_proto::method all_methods =
        http_proto::method::unknown;

public:
    /** Constructor
    */
    router()
        : router_base(
            [](void* req) -> http_proto::method
            {
                return reinterpret_cast<Request*>(req)->method;
            },
            [](void* req) -> urls::segments_encoded_view&
            {
                return reinterpret_cast<Request*>(req)->path;
            })
    {
    }

    /** Add a global middleware
        The handler will run for every request.
    */
    template<class H0, class... HN
    , class = typename std::enable_if<
        ! std::is_convertible<H0, core::string_view>::value>::type
    >
    void use(H0&& h0, HN&&... hn)
    {
        append(true, all_methods, "/",
            std::forward<H0>(h0),
            std::forward<HN>(hn)...);
    }

    /** Add a mounted middleware
        The handler will run for every request matching the given prefix.
    */
    template<class H0, class... HN>
    void use(core::string_view pattern,
        H0&& h0, HN... hn)
    {
        append(true, all_methods, pattern,
            std::forward<H0>(h0),
            std::forward<HN>(hn)...);
    }

    template<class H0, class... HN>
    void add(
        http_proto::method method,
        core::string_view pattern,
        H0&& h0, HN&&... hn)
    {
        append(false, method, pattern,
            std::forward<H0>(h0),
            std::forward<HN>(hn)...);
    }

    /** Add an error handler
    */
    template<class H0, class... HN>
    void err(
        H0&& h0, HN&&... hn)
    {
        append_err(
            std::forward<H0>(h0),
            std::forward<HN>(hn)...);
    }

    /** Add a route handler matching all methods and the given pattern
        The handler will run for every request matching the entire pattern.
    */
    template<class H0, class... HN>
    void all(
        core::string_view pattern,
        H0&& h0, HN&&... hn)
    {
        return add(all_methods, pattern,
            std::forward<H0>(h0), std::forward<HN>(hn)...);
    }

    /** Add a GET handler
    */
    template<class H0, class... HN>
    void get(
        core::string_view pattern,
        H0&& h0, HN&&... hn)
    {
        add(http_proto::method::get, pattern,
            std::forward<H0>(h0), std::forward<HN>(hn)...);
    }

    template<class H0, class... HN>
    void post(
        core::string_view pattern,
        H0&& h0, HN&&... hn)
    {
        add(http_proto::method::post, pattern,
            std::forward<H0>(h0), std::forward<HN>(hn)...);
    }

    auto
    operator()(
        Request& req,
        Response& res,
        route_state& st) const ->
            system::error_code 
    {
        return invoke(&req, &res, st);
    }

    auto
    resume(
        Request& req,
        Response& res,
        system::error_code const& ec,
        route_state& st) const ->
            system::error_code
    {
        st.pos = 0;
        st.ec = ec;
        return invoke(&req, &res, st);
    }

private:
    void append(bool, http_proto::method,
        core::string_view ) const noexcept
    {
    }

    template<class H0, class... HN>
    void append(bool prefix, http_proto::method method,
        core::string_view pat, H0&& h, HN&&... hn)
    {
        router_base::append(prefix, method, pat,
            handler_ptr(new handler_impl<Request, Response, H0>(
                std::forward<H0>(h))));
        append(prefix, method, pat, std::forward<HN>(hn)...);
    }

    void append_err() const noexcept
    {
    }

    template<class H0, class... HN>
    void append_err(H0&& h, HN&&... hn)
    {
        router_base::append_err(errfn_ptr(new
            errfn_impl<Request, Response, H0>(
                std::forward<H0>(h))));
        append_err(std::forward<HN>(hn)...);
    }

};

//------------------------------------------------

struct BOOST_SYMBOL_VISIBLE
    router_base::any_handler
{
    // total children including this one
    std::size_t n_routes;

    BOOST_BEAST2_DECL
    virtual ~any_handler();

    virtual system::error_code operator()(
        void*, void*, route_state&) const = 0;
};

struct BOOST_SYMBOL_VISIBLE
    router_base::any_errfn
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
struct router_base::handler_impl : any_handler
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
struct router_base::handler_impl<Request, Response, Handler,
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
struct router_base::errfn_impl : any_errfn
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

} // beast2
} // boost

#endif
