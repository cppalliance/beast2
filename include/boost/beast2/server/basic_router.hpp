//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_BASIC_ROUTER_HPP
#define BOOST_BEAST2_SERVER_BASIC_ROUTER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/detail/any_router.hpp>
#include <boost/beast2/detail/call_traits.hpp>
#include <boost/beast2/detail/type_traits.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

struct route_state
{
private:
    friend class detail::any_router;
    template<class Res, class Req>
    friend class basic_router;

    std::size_t pos = 0;
    std::size_t resume = 0;
    system::error_code ec;
};

//------------------------------------------------

/** A container for HTTP route handlers
    The basic_router class template is used to
    store and invoke route handlers based on
    the request method and path.
    Handlers are added to the router using
    the member functions such as @ref get,
    @ref post, and @ref all.
    Handlers are invoked by calling the
    function call operator with a request
    and response object.
    @tparam Request The type of request object.
    @tparam Response The type of response object.
    @par Example
    @code
    using router_type = basic_router<Request, Response>;
    router_type router;
    router.get("/hello",
        [](Request& req, Response& res)
        {
            res.status(http_proto::status::ok);
            res.set_body("Hello, world!");
            return system::error_code{};
        });
    @endcode
*/
template<class Request, class Response>
class basic_router : public detail::any_router
{
    static constexpr http_proto::method all_methods =
        http_proto::method::unknown;

public:
    /** The type of request object used in handlers

        Route handlers must have this invocable signature
        @code
        system::error_code(Request&, Response&)
        @endcode
    */
    using request_type = Request;

    /** The type of response object used in handlers

        Route handlers must have this invocable signature
        @code
        system::error_code(Request&, Response&)
        @endcode
    */
    using response_type = Response;

    /** Constructor
    */
    basic_router()
        : any_router(
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

    /** Constructor
    */
    template<
        class DerivedRequest, class DerivedResponse,
        class = typename std::enable_if<
            detail::derived_from<Request, DerivedRequest>::value &&
            detail::derived_from<Response, DerivedResponse>::value>::type
    >
    basic_router(
        basic_router<DerivedRequest, DerivedResponse> const& other)
        : any_router(other)
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
        any_router::append(prefix, method, pat,
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
        any_router::append_err(errfn_ptr(new
            errfn_impl<Request, Response, H0>(
                std::forward<H0>(h))));
        append_err(std::forward<HN>(hn)...);
    }

};

} // beast2
} // boost

#endif
