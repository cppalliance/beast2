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
#include <boost/beast2/detail/call_traits.hpp>
#include <boost/beast2/detail/type_traits.hpp>
#include <boost/beast2/server/router_state.hpp>
#include <boost/beast2/server/route_handler.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/url/url_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

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

// route_result( Req&, Res&, basic_response& )
template<class T, class Req, class Res>
struct get_handler_kind<T, Req, Res,
    typename std::enable_if<detail::derived_from<
        any_router, T>::value>::type>
    : std::integral_constant<int, 2>
{
};

// route_result( Req&, Res&, system::error_code )
template<class T, class Req, class Res>
struct get_handler_kind<T, Req, Res,
    typename std::enable_if<detail::is_invocable<
        T, route_result, Req&, Res&,
            system::error_code const>::value>::type>
    : std::integral_constant<int, 3>
{
};

template<class Req, class Res, class T>
struct is_error_handler
    : std::integral_constant<bool,
    get_handler_kind<T, Req, Res>::value == 3>
{
};

template<class Req, class Res, class... HN>
struct is_error_handlers
    : all_true<is_error_handler<Req, Res, typename
            std::decay<HN>::type>::value...>
{
};

template<class Req, class Res, class T>
struct is_route_handler
    : std::integral_constant<bool,
    get_handler_kind<T, Req, Res>::value == 1>
{
};

template<class Req, class Res, class... HN>
struct is_route_handlers
    : all_true<is_route_handler<Req, Res, typename
            std::decay<HN>::type>::value...>
{
};

//------------------------------------------------

// implementation for all routers
class any_router
{
public:
    void operator()() = delete;

protected:
    static constexpr http_proto::method
        middleware = http_proto::method::unknown;

    struct any_handler;
    using handler_ptr = std::unique_ptr<any_handler>;
    using match_result = basic_request::match_result;
    struct matcher;
    struct layer;
    struct impl;

    struct handler_list
    {
        std::size_t n;
        handler_ptr* p;
    };

    BOOST_BEAST2_DECL ~any_router();
    BOOST_BEAST2_DECL any_router();
    BOOST_BEAST2_DECL any_router(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router(any_router const&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router const&) noexcept;

    BOOST_BEAST2_DECL std::size_t count() const noexcept;

    BOOST_BEAST2_DECL
    route_result
    dispatch_impl(
        http_proto::method,
        urls::url_view const&,
        basic_request&, basic_response&) const;

    BOOST_BEAST2_DECL
    route_result
    dispatch_impl(
        basic_request&, basic_response&) const;

    // VFALCO consider using cached request and response references
    BOOST_BEAST2_DECL
    route_result
    resume(
        basic_request&, basic_response&,
        route_result const& ec) const;

    BOOST_BEAST2_DECL void use_impl(
        core::string_view, handler_list const&);
    BOOST_BEAST2_DECL layer& make_route(
        core::string_view pattern);
    BOOST_BEAST2_DECL void append_impl(layer&,
        http_proto::method, handler_list const&);

    impl* impl_ = nullptr;
};

//------------------------------------------------

struct BOOST_SYMBOL_VISIBLE
    any_router::any_handler
{
    BOOST_BEAST2_DECL virtual ~any_handler();
    virtual std::size_t count() const noexcept = 0;
    virtual route_result invoke(
        basic_request&, basic_response&) const = 0;
};

//------------------------------------------------

} // detail

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

    Express treats all route definitions as decoded path patterns, not raw
    URL-encoded ones. So a literal %2F in the pattern string is indistinguishable
    from a literal / once Express builds the layer. Therefore "/x%2Fz" is
    the same as "/x/z"

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
    BOOST_CORE_STATIC_ASSERT(
        detail::derived_from<basic_response, Res>::value);

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

    /** A fluent interface for defining handlers on a specific route.

        This type represents a single route within the router and
        provides a chainable API for registering handlers associated
        with particular HTTP methods or for all methods collectively.
        Instances of this type are returned by @ref basic_router::route
        and remain valid for the lifetime of the parent router.

        Typical usage registers one or more handlers for a route:
        @code
        router.route("/users/:id")
            .get(show_user)
            .put(update_user)
            .all(log_access);
        @endcode

        Each call appends handlers in registration order, preserving
        the same semantics as Express.js routes.
    */
    class fluent_route;

    /** Constructor
    */
    basic_router() = default;

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
        use(core::string_view(),
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    /** Register one or more global error handlers.

        Installs error handlers that are invoked when an unhandled
        error occurs during request processing, regardless of the
        request path. Handlers execute in the order of registration
        and are called only when the response contains a failing
        error code.

        Each handler may return any failing @ref system::error_code,
        which is equivalent to calling @code res.next(ec); @endcode
        with `ec.failed() == true`. Returning @ref route::next
        indicates that control should proceed to the next matching
        error handler. Returning a different failing code replaces
        the current error and continues dispatch in error mode
        using that new code.

        This overload registers the handlers at the root scope (`"/"`),
        equivalent to calling @ref err(core::string_view, HN&&...) with
        a path argument of `"/"`.

        @param h1 The first error handler to install.
        @param hn Additional error handlers to install, invoked in
        declaration order.

        @par Handler requirements
        Each handler must be callable with the signature:
        @code
        route_result(Request&, Response&, system::error_code)
        @endcode
        The handler is invoked only when the response has a failing
        error code. Handlers execute in sequence until one returns a
        result other than @ref route::next.
    */
    template<class H1, class... HN
        , class = typename std::enable_if<! std::is_convertible<
            H1, core::string_view>::value>::type
    >
    void err(H1&& h1, HN&&... hn)
    {
        err(core::string_view(),
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
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
        @param h1  The first middleware handler to install.
        @param hn  Additional middleware handlers to install, executed in
            declaration order.

        @return (none)
    */
    template<class H1, class... HN>
    void use(
        core::string_view pattern,
        H1&& h1, HN... hn)
    {
        use_impl(pattern,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
    }

    /** Register one or more error handlers for a path prefix.

        Adds error handlers that are invoked when an unhandled error
        occurs during request processing for any request whose path
        begins with the specified prefix. Handlers execute in the order
        of registration and are called only when the response contains
        a failing error code.

        Each handler may return any failing @ref system::error_code,
        which is equivalent to calling @code res.next(ec); @endcode
        with `ec.failed() == true`. Returning @ref route::next indicates
        that control should proceed to the next matching error handler.
        Returning a different failing code replaces the current error
        and continues dispatch in error mode using that new code.

        @param pattern The path prefix to match. Error handlers run for
        any request whose path begins with this prefix. If the pattern
        is empty, it defaults to `"/"`.
        @param h1 The first error handler to install.
        @param hn Additional error handlers to install, invoked in
        registration order.

        @par Handler requirements
        Each handler must be callable with the signature:
        @code
        route_result(Request&, Response&, system::error_code)
        @endcode
        The handler is invoked only when the response has a failing
        error code. Handlers execute in sequence until one returns a
        result other than @ref route::next.
    */
    template<class H1, class... HN>
    void err(
        core::string_view pattern,
        H1&& h1, HN... hn)
    {
        // All handlers must have the error handler signature
        BOOST_STATIC_ASSERT(
            detail::is_error_handlers<Req, Res, H1, HN...>::value);
        use_impl(pattern,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
    }

    /** Create a new route for the specified path pattern.

        Adds a new route to the router for the given pattern.
        A new route object is always created, even if another
        route with the same pattern already exists. The returned
        @ref fluent_route reference allows method-specific handler
        registration (such as GET or POST) or catch-all handlers
        with @ref fluent_route::all.

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @return A reference to a fluent route interface for chaining
        handler registrations.
    */
    auto
    route(
        core::string_view pattern) -> fluent_route
    {
        return fluent_route(*this, pattern);
    }

    template<class H1, class... HN>
    void add(
        http_proto::method method,
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        append(false, method, pattern,
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    template<class H1, class... HN>
    auto all(
        core::string_view pattern,
        H1&& h1, HN&&... hn) -> fluent_route
    {
        return this->route(pattern).all(
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    /** Register handlers for HTTP GET requests matching a path pattern.

        Creates a new route for the specified pattern and attaches
        one or more handlers that will be invoked for incoming HEAD
        requests whose targets match that pattern. A new route object
        is always created, even if another route with the same pattern
        already exists. Handlers are executed in the order of
        registration.

        This function returns a @ref fluent_route
        reference, allowing additional method registrations to be
        chained on the same route. For example:
        @code
        router.route("/status")
            .head(check_headers)
            .get(send_status);
        @endcode

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto get(
        core::string_view pattern,
        H1&& h1, HN&&... hn) -> fluent_route
    {
        return this->route(pattern).get(
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    /** Register handlers for HTTP HEAD requests matching a path pattern.

        Creates a new route for the specified pattern and attaches
        one or more handlers that will be invoked for incoming HEAD
        requests whose targets match that pattern. A new route object
        is always created, even if another route with the same pattern
        already exists. Handlers are executed in the order of
        registration.

        This function returns a @ref fluent_route
        reference, allowing additional method registrations to be
        chained on the same route. For example:
        @code
        router.route("/status")
            .head(check_headers)
            .get(send_status);
        @endcode

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto head(
        core::string_view pattern,
        H1&& h1, HN&&... hn) -> fluent_route
    {
        return this->route(pattern).head(
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    /** Register handlers for HTTP POST requests matching a path pattern.

        Creates a new route for the specified pattern and attaches
        one or more handlers that will be invoked for incoming POST
        requests whose targets match that pattern. A new route object
        is always created, even if another route with the same pattern
        already exists. Handlers are executed in the order of
        registration.

        The returned @ref fluent_route reference allows additional
        method registrations to be chained on the same route. For
        example:
        @code
        router.route("/submit")
            .post(process_form)
            .get(show_form);
        @endcode

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto post(
        core::string_view pattern,
        H1&& h1, HN&&... hn) -> fluent_route
    {
        return this->route(pattern).post(
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    //--------------------------------------------

    auto
    dispatch(
        http_proto::method method,
        urls::url_view const& url,
        Req& req, Res& res) ->
            route_result
    {
        return dispatch_impl(
            method, url, req, res);
    }

    auto
    resume(
        Req& req,
        Res& res,
        route_result const& ec) const ->
            route_result
    {
#if 0
        st.pos = 0;
        st.ec = ec;
        return dispatch_impl(&req, res);
#endif
        (void)req;
        (void)res;
        (void)ec;
        return beast2::route::close; // VFALCO FIXME
    }

private:
    template<class T>
    struct handler_impl;

    template<std::size_t N>
    struct handler_list_impl : handler_list
    {
        template<class... HN>
        explicit handler_list_impl(HN&&... hn)
        {
            n = sizeof...(HN);
            p = v;
            assign<0>(std::forward<HN>(hn)...);
        }

    private:
        template<std::size_t I, class H1, class... HN>
        void assign(H1&& h1, HN&&... hn)
        {
            v[I] = handler_ptr(new handler_impl<H1>(
                std::forward<H1>(h1)));
            assign<I+1>(std::forward<HN>(hn)...);
        }

        template<std::size_t>
        void assign()
        {
        }

        handler_ptr v[N];
    };

    template<class... HN>
    static auto
    make_handler_list(HN&&... hn) ->
        handler_list_impl<sizeof...(HN)>
    {
        return handler_list_impl<sizeof...(HN)>(
            std::forward<HN>(hn)...);
    }

    void append(layer& e,
        http_proto::method verb,
        handler_list const& handlers)
    {
        append_impl(e, verb, handlers);
    }
};

//------------------------------------------------

// wrapper for route handlers
template<class Req, class Res>
template<class H>
struct basic_router<Req, Res>::
    handler_impl : any_handler
{
    typename std::decay<H>::type h;

    template<class... Args>
    explicit handler_impl(Args&&... args)
        : h(std::forward<Args>(args)...)
    {
    }

    std::size_t
    count() const noexcept override
    {
        return count(
            detail::get_handler_kind<
                decltype(h), Req, Res>{});
    }

    route_result
    invoke(
        basic_request& req,
        basic_response& res) const override
    {
        return invoke(
            static_cast<Req&>(req),
            static_cast<Res&>(res),
            detail::get_handler_kind<
                decltype(h), Req, Res>{});
    }

private:
    std::size_t count(
        std::integral_constant<int, 0>) = delete;

    std::size_t count(
        std::integral_constant<int, 1>) const noexcept
    {
        return 1;
    }

    std::size_t count(
        std::integral_constant<int, 2>) const noexcept
    {
        return 1 + h.count();
    }

    std::size_t count(
        std::integral_constant<int, 3>) const noexcept
    {
        return 1;
    }

    route_result invoke(Req&, Res&,
        std::integral_constant<int, 0>) const = delete;

    // (Req, Res)
    route_result invoke(Req& req, Res& res,
        std::integral_constant<int, 1>) const
    {
        auto const& ec = static_cast<
            basic_response const&>(res).ec_;
        if(ec.failed())
            return beast2::route::next;
        // avoid racing on res.resume_
        res.resume_ = res.pos_;
        auto rv = h(req, res);
        if(rv == beast2::route::detach)
            return rv;
        res.resume_ = 0; // revert
        return rv;
    }

    // any_router
    route_result invoke(Req& req, Res& res,
        std::integral_constant<int, 2>) const
    {
        auto const& ec = static_cast<
            basic_response const&>(res).ec_;
        if(! ec.failed())
            return h.dispatch_impl(req, res);
        return beast2::route::next;
    }

    // (Req&, Res&, error_code)
    route_result
    invoke(Req& req, Res& res,
        std::integral_constant<int, 3>) const
    {
        auto const& ec = static_cast<
            basic_response const&>(res).ec_;
        if(! ec.failed())
            return beast2::route::next;
        // avoid racing on res.resume_
        res.resume_ = res.pos_;
        auto rv = h(req, res, ec);
        if(rv == beast2::route::detach)
            return rv;
        res.resume_ = 0; // revert
        return rv;
    }
};

//------------------------------------------------

template<class Req, class Res>
class basic_router<Req, Res>::
    fluent_route
{
public:
    fluent_route(fluent_route const&) = default;

    /** Register handlers for a specific HTTP method.

        Adds one or more handlers to be invoked when a request matches
        this route and uses the specified HTTP method. The method must
        not be @ref http_proto::method::unknown; attempting to register
        handlers with that value will throw an exception.

        Handlers are executed in the order of registration relative to
        other handlers for the same route and method. The function
        returns a reference to the current @ref fluent_route, allowing
        additional method registrations to be chained. For example:
        @code
        router.route("/item")
            .add(http_proto::method::put, update_item)
            .add(http_proto::method::delete_, remove_item);
        @endcode

        @param verb The HTTP method for which to register the handlers.
        @param h1 The first handler to invoke when the route and method
        both match.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
        @throws system_error Thrown if @p verb is
        @ref http_proto::method::unknown.
    */
    template<class H1, class... HN>
    auto add(
        http_proto::method verb,
        H1&& h1, HN&&... hn) ->
            fluent_route&
    {
        if(verb == http_proto::method::unknown)
            detail::throw_invalid_argument();
        owner_.append(e_, verb,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
        return *this;
    }

    /** Register handlers that apply to all HTTP methods.

        Adds one or more handlers that will be invoked for any request
        whose method is not matched by a more specific registration.
        These handlers run in the order of registration relative to
        other method-specific handlers on the same route.

        This function returns a reference to the current
        @ref fluent_route, allowing additional method registrations
        to be chained. For example:
        @code
        router.route("/resource")
            .all(log_request)
            .get(show_resource)
            .post(update_resource);
        @endcode

        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto all(
        H1&& h1, HN&&... hn) ->
            fluent_route&
    {
        owner_.append(e_, http_proto::method::unknown,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
        return *this;
    }

    /** Register handlers for HTTP GET requests matching a path pattern.

        Creates a new route for the specified pattern and attaches
        one or more handlers that will be invoked for incoming HEAD
        requests whose targets match that pattern. A new route object
        is always created, even if another route with the same pattern
        already exists. Handlers are executed in the order of
        registration.

        This function returns a @ref fluent_route
        reference, allowing additional method registrations to be
        chained on the same route. For example:
        @code
        router.route("/status")
            .head(check_headers)
            .get(send_status);
        @endcode

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto get(
        H1&& h1, HN&&... hn) ->
            fluent_route&
    {
        owner_.append(e_, http_proto::method::get,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
        return *this;
    }

    /** Register handlers for HTTP HEAD requests matching a path pattern.

        Creates a new route for the specified pattern and attaches
        one or more handlers that will be invoked for incoming HEAD
        requests whose targets match that pattern. A new route object
        is always created, even if another route with the same pattern
        already exists. Handlers are executed in the order of
        registration.

        This function returns a @ref fluent_route
        reference, allowing additional method registrations to be
        chained on the same route. For example:
        @code
        router.route("/status")
            .head(check_headers)
            .get(send_status);
        @endcode

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto head(
        H1&& h1, HN&&... hn) ->
            fluent_route&
    {
        owner_.append(e_, http_proto::method::head,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
        return *this;
    }

    /** Register handlers for HTTP POST requests matching a path pattern.

        Creates a new route for the specified pattern and attaches
        one or more handlers that will be invoked for incoming POST
        requests whose targets match that pattern. A new route object
        is always created, even if another route with the same pattern
        already exists. Handlers are executed in the order of
        registration.

        The returned @ref fluent_route reference allows additional
        method registrations to be chained on the same route. For
        example:
        @code
        router.route("/submit")
            .post(process_form)
            .get(show_form);
        @endcode

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return A reference to the fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto post(
        H1&& h1, HN&&... hn) ->
            fluent_route&
    {
        owner_.append(e_, http_proto::method::post,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
        return *this;
    }

private:
    friend class basic_router;
    fluent_route(
        basic_router& owner,
        core::string_view pattern)
        : e_(owner.make_route(pattern))
        , owner_(owner)
    {
    }

    layer& e_;
    basic_router& owner_;
};

} // beast2
} // boost

#endif
