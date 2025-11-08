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
#include <boost/beast2/server/route_handler.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

struct route_state;

//------------------------------------------------

namespace detail {

class any_router;

// handler kind constants
// 0 = unrecognized
// 1 = regular
// 2 = router
// 3 = error

template<class T,
    class Req, class Res, class = void>
struct get_handler_kind
    : std::integral_constant<int, 0>
{
};

// route_result( Req&, Res& )
template<class T, class Req, class Res>
struct get_handler_kind<T, Req, Res,
    typename std::enable_if<detail::is_invocable<
        T, route_result, Req&, Res&>::value>::type>
    : std::integral_constant<int, 1>
{
};

// route_result( Req&, Res&, route_state& )
template<class T, class Req, class Res>
struct get_handler_kind<T, Req, Res,
    typename std::enable_if<detail::derived_from<
        T, any_router>::value>::type>
    : std::integral_constant<int, 2>
{
};

// route_result( Req&, Res&, system::error_code& 
template<class T, class Req, class Res>
struct get_handler_kind<T, Req, Res,
    typename std::enable_if<detail::is_invocable<
        T, route_result, Req&, Res&,
            system::error_code&>::value>::type>
    : std::integral_constant<int, 3>
{
};

//------------------------------------------------

// implementation for all routers
class any_router
{
protected:
    struct layer;
    struct impl;
    struct any_handler;

    struct req_info
    {
        core::string_view& base_path;
        core::string_view& path;
    };

    using handler_ptr = std::unique_ptr<any_handler>;

    template<class, class, class>
    struct handler_impl;

    any_router() = default;

    BOOST_BEAST2_DECL ~any_router();
    BOOST_BEAST2_DECL any_router(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router(any_router const&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router const&) noexcept;
    BOOST_BEAST2_DECL any_router(req_info(*)(void*));
    BOOST_BEAST2_DECL std::size_t count() const noexcept;
    BOOST_BEAST2_DECL route_result dispatch_impl(http_proto::method,
        urls::url_view const&, void*, void*, route_state& st) const;
    BOOST_BEAST2_DECL route_result dispatch_impl(
        void*, void*, route_state&) const;
    BOOST_BEAST2_DECL route_result resume(
        void*, void*, route_state&, route_result const& ec) const;
    BOOST_BEAST2_DECL void append(bool, http_proto::method,
        core::string_view, handler_ptr);
    BOOST_BEAST2_DECL void append(core::string_view, any_router&);
    void append(bool, http_proto::method, core::string_view) {}

    impl* impl_ = nullptr;
};

//------------------------------------------------

struct BOOST_SYMBOL_VISIBLE
    any_router::any_handler
{
    BOOST_BEAST2_DECL virtual ~any_handler();
    BOOST_BEAST2_DECL virtual
        std::size_t count() const noexcept = 0 ;
    virtual route_result invoke(
        void*, void*, route_state&,
            system::error_code*) const = 0;
};

// wrapper for route handlers
template<class Req, class Res, class H>
struct any_router::
    handler_impl : any_handler
{
    typename std::decay<H>::type h;

    template<class... Args>
    handler_impl(Args&&... args)
        : h(std::forward<Args>(args)...)
    {
    }

    std::size_t
    count() const noexcept override
    {
        return count(get_handler_kind<
            decltype(h), Req, Res>{});
    }

    route_result
    invoke(
        void* req, void* res,
        route_state& st,
        system::error_code* pec) const override
    {
        return invoke(
            *reinterpret_cast<Req*>(req),
            *reinterpret_cast<Res*>(res),
            st, pec, get_handler_kind<
                decltype(h), Req, Res>{});
    }

private:
    std::size_t count(
        std::integral_constant<int, 0>) const = delete;

    std::size_t count(
        std::integral_constant<int, 1>) const noexcept
    {
        return 1;
    }

    std::size_t count(
        std::integral_constant<int, 2>) const noexcept
    {
        return h.count();
    }

    std::size_t count(
        std::integral_constant<int, 3>) const noexcept
    {
        return 1;
    }

    route_result
    invoke(Req&, Res&, route_state&,
        system::error_code*,
        std::integral_constant<int, 0>) const = delete;

    route_result
    invoke(Req& req, Res& res, route_state&,
        system::error_code* pec,
        std::integral_constant<int, 1>) const
    {
        if(! pec)
            return h(req, res);
        return route::next;
    }

    route_result
    invoke(Req& req, Res& res, route_state& st,
        system::error_code* pec,
        std::integral_constant<int, 2>) const
    {
        if(! pec)
            return h.dispatch(&req, &res, st);
        return route::next;
    }

    route_result
    invoke(Req& req, Res& res, route_state&,
        system::error_code* pec,
        std::integral_constant<int, 3>) const
    {
        if(pec != nullptr)
            return h(req, res, *pec);
        return route::next;
    }
};

} // detail
} // beast2
} // boost

#endif
