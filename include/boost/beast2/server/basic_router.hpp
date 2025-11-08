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
#include <boost/url/url_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

struct route_state
{
private:
    friend class detail::any_router;
    template<class, class>
    friend class basic_router;

    std::size_t pos = 0;
    std::size_t resume = 0;
    route_result ec;
    http_proto::method verb;
    core::string_view verb_str;
    std::string decoded_path;
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

    Express treats all route definitions as decoded path patterns, not raw URL-encoded ones.
    So a literal %2F in the pattern string is indistinguishable from a literal / once Express builds the layer.
    Therefore "/x%2Fz" is the same as "/x/z"

    @par Example
    @code
    using router_type = basic_router<Req, Res>;
    router_type router;
    router.get("/hello",
        [](Req& req, Res& res)
        {
            res.status(http_proto::status::ok);
            res.set_body("Hello, world!");
            return system::error_code{};
        });
    @endcode

    @tparam Req The type of request object.
    @tparam Res The type of response object.
*/
template<class Req, class Res>
class basic_router : public detail::any_router
{
    static constexpr http_proto::method
        middleware = http_proto::method::unknown;

public:
    /** The type of request object used in handlers

        Route handlers must have this invocable signature
        @code
        system::error_code(Req&, Res&)
        @endcode
    */
    using request_type = Req;

    /** The type of response object used in handlers

        Route handlers must have this invocable signature
        @code
        system::error_code(Req&, Res&)
        @endcode
    */
    using response_type = Res;

    /** Constructor
    */
    basic_router()
        : any_router(
            [](void* preq) -> req_info
            {
                auto& req = *reinterpret_cast<Req*>(preq);
                return req_info{ req.base_path, req.path };
            })
    {
    }

    /** Constructor
    */
    template<
        class OtherReq, class OtherRes,
        class = typename std::enable_if<
            detail::derived_from<Req, OtherReq>::value &&
            detail::derived_from<Res, OtherRes>::value>::type
    >
    basic_router(
        basic_router<OtherReq, OtherRes> const& other)
        : any_router(other)
    {
    }

    /** Add one or more global middleware handlers.

        Each handler registered with this function runs for every incoming
        request, regardless of its HTTP method or path. Handlers execute in
        the order they were added, and may call `next()` to transfer control
        to the subsequent handler in the chain.

        This is equivalent to writing
        @code
        use( "/", h1, hn... );
        @endcode
    */
    template<class H1, class... HN
        , class = typename std::enable_if<! std::is_convertible<
            H1, core::string_view>::value>::type
    >
    void use(H1&& h1, HN&&... hn)
    {
        use("/", std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more middleware handlers for a path prefix.

        Each handler registered with this function runs for every request
        whose path begins with the specified prefix, regardless of the
        request method. The prefix match is not strict: middleware attached
        to `"/api"` will also match `"/api/users"` and `"/api/data"`.
        Handlers execute in the order they were added, and may call `next()`
        to transfer control to the subsequent handler.

        This function behaves analogously to `app.use(path, ...)` in
        Express.js. The registered middleware executes for requests matching
        the prefix, and when registered before route handlers for the same
        prefix, runs prior to those routes.

        @par Example
        @code
        router.use("/api",
            [](Request& req, Response& res)
            {
                if(!authenticate(req))
                    return res.setStatus(401), res.end("Unauthorized");
                return res.next();
            },
            [](Request&, Response& res)
            {
                res.setHeader("X-Powered-By", "MyServer");
            });
        @endcode

        @par Constraints
        - `pattern` must be a valid path prefix; it may be empty to indicate
          the root scope.
        - Each handler must be callable with the signature  
          `void(Request&, Response&, NextHandler)`.
        - Each handler must be copy- or move-constructible, depending on how
          it is passed.

        @throws Any exception thrown by a handler during execution.

        @param pattern  The path prefix to match. Middleware runs for any
            request whose path begins with this prefix.
        @param h0  The first middleware handler to install.
        @param hn  Additional middleware handlers to install, executed in
            declaration order.

        @return (none)
    */
    template<class H0, class... HN>
    void use(
        core::string_view pattern,
        H0&& h0, HN... hn)
    {
        if(pattern.empty())
            pattern = "/";
        append(false, middleware, pattern,
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
        return add(middleware, pattern,
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

    auto dispatch(
        http_proto::method method,
        urls::url_view const& url,
        Req& req, Res& res, route_state& st) ->
            route_result
    {
        return dispatch_impl(
            method, url, &req, &res, st);
    }

    auto
    resume(
        Req& req,
        Res& res,
        route_result const& ec,
        route_state& st) const ->
            route_result
    {
        st.pos = 0;
        st.ec = ec;
        return dispatch_impl(&req, &res, st);
    }

private:
    template<class H>
    handler_ptr make_handler(H&& h)
    {
        return handler_ptr(new handler_impl<
            Req, Res, H>(std::forward<H>(h)));
    }

    void append(bool, http_proto::method,
        core::string_view) const noexcept
    {
    }

    template<class H0, class... HN>
    void append(bool prefix, http_proto::method method,
        core::string_view pat, H0&& h, HN&&... hn)
    {
        any_router::append(prefix, method, pat,
            make_handler<H0>(std::forward<H0>(h)));
        append(prefix, method, pat, std::forward<HN>(hn)...);
    }

    template<class H0, class... HN>
    void append_err(H0&& h, HN&&... hn)
    {
        append(true, middleware, {},
            std::forward<H0>(h),
            std::forward<HN>(hn)...);
    }
};

} // beast2
} // boost

#endif
