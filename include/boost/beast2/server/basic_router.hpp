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
#include <boost/beast2/server/router_types.hpp>
#include <boost/beast2/server/route_handler.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/url/url_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

//-----------------------------------------------

namespace detail {

class any_router;

// handler kind constants
// 0 = unrecognized
// 1 = normal handler (Req&, Res&)
// 2 = error handler  (Req&, Res&, error_code)
// 3 = sub-router

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

// route_result( Req&, Res&, system::error_code )
template<class T, class Req, class Res>
struct get_handler_kind<T, Req, Res,
    typename std::enable_if<detail::is_invocable<
        T, route_result, Req&, Res&,
            system::error_code const>::value>::type>
    : std::integral_constant<int, 2>
{
};

// route_result( Req&, Res&, basic_response& )
template<class T, class Req, class Res>
struct get_handler_kind<T, Req, Res,
    typename std::enable_if<detail::derived_from<
        any_router, T>::value>::type>
    : std::integral_constant<int, 3>
{
};

template<class Req, class Res, class T>
struct is_handler
    : std::integral_constant<bool,
    get_handler_kind<T, Req, Res>::value == 1>
{
};

template<class Req, class Res, class... HN>
struct is_handlers
    : all_true<is_handler<Req, Res, typename
            std::decay<HN>::type>::value...>
{
};

template<class Req, class Res, class T>
struct is_error_handler
    : std::integral_constant<bool,
    get_handler_kind<T, Req, Res>::value == 2>
{
};

template<class Req, class Res, class... HN>
struct is_error_handlers
    : all_true<is_error_handler<Req, Res, typename
            std::decay<HN>::type>::value...>
{
};

//-----------------------------------------------

// implementation for all routers
class any_router
{
protected:
    struct BOOST_SYMBOL_VISIBLE any_handler
    {
        BOOST_BEAST2_DECL virtual ~any_handler();
        virtual std::size_t count() const noexcept = 0;
        virtual route_result invoke(
            basic_request&, basic_response&) const = 0;
    };

    using handler_ptr = std::unique_ptr<any_handler>;

    struct handler_list
    {
        std::size_t n;
        handler_ptr* p;
    };

    using match_result = basic_request::match_result;
    struct matcher;
    struct layer;
    struct impl;

    BOOST_BEAST2_DECL ~any_router();
    BOOST_BEAST2_DECL any_router();
    BOOST_BEAST2_DECL any_router(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router(any_router const&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router&&) noexcept;
    BOOST_BEAST2_DECL any_router& operator=(any_router const&) noexcept;
    BOOST_BEAST2_DECL std::size_t count() const noexcept;
    BOOST_BEAST2_DECL layer& new_layer(core::string_view pattern);
    BOOST_BEAST2_DECL void add_impl(core::string_view, handler_list const&);
    BOOST_BEAST2_DECL void add_impl(layer&,
        http_proto::method, handler_list const&);
    BOOST_BEAST2_DECL void add_impl(layer&,
        core::string_view, handler_list const&);
    BOOST_BEAST2_DECL route_result resume_impl(
        basic_request&, basic_response&, route_result const& ec) const;
    BOOST_BEAST2_DECL route_result dispatch_impl(http_proto::method,
        core::string_view, urls::url_view const&,
            basic_request&, basic_response&) const;
    BOOST_BEAST2_DECL route_result dispatch_impl(
        basic_request&, basic_response&) const;
    route_result do_dispatch(basic_request&, basic_response&) const;

    impl* impl_ = nullptr;
};

} // detail

//-----------------------------------------------

/** A container for HTTP route handlers.

    `basic_router` objects store and dispatch route handlers based on the
    HTTP method and path of an incoming request. Routes are added with a
    path pattern and an associated handler, and the router is then used to
    dispatch the appropriate handler.

    Patterns used to create route definitions have percent-decoding applied
    when handlers are mounted. A literal "%2F" in the pattern string is
    indistinguishable from a literal '/'. For example, "/x%2Fz" is the
    same as "/x/z" when used as a pattern.

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

    Router objects are lightweight, shared references to their contents.
    Copies of a router obtained through construction, conversion, or
    assignment do not create new instances; they all refer to the same
    underlying data.

    @par Handlers
    Regular handlers are invoked for matching routes and have this
    equivalent signature:
    @code
    route_result handler( Req& req, Res& res )
    @endcode

    The return value is a @ref route_result used to indicate the desired
    action through @ref route enum values, or to indicate that a failure
    occurred. Failures are represented by error codes for which
    `system::error_code::failed()` returns `true`.

    When a failing error code is produced and remains unhandled, the
    router enters error-dispatching mode. In this mode, only error
    handlers are invoked. Error handlers are registered globally or
    for specific paths and execute in the order of registration whenever
    a failing error code is present in the response.

    Error handlers have this equivalent signature:
    @code
    route_result error_handler( Req& req, Res& res, system::error_code ec )
    @endcode

    Each error handler may return any failing @ref system::error_code,
    which is equivalent to calling:
    @code
    res.next(ec); // with ec.failed() == true
    @endcode
    Returning @ref route::next indicates that control should proceed to
    the next matching error handler. Returning a different failing code
    replaces the current error and continues dispatch in error mode using
    that new code. Error handlers are invoked until one returns a result
    other than @ref route::next.

    @par Handler requirements
    Regular handlers must be callable with:
    @code
    route_result( Req&, Res& )
    @endcode

    Error handlers must be callable with:
    @code
    route_result( Req&, Res&, system::error_code )
    @endcode
    Error handlers are invoked only when the response has a failing
    error code, and execute in sequence until one returns a result
    other than @ref route::next.

    @par Thread Safety
    Member functions marked `const` such as @ref dispatch and @ref resume
    may be called concurrently on routers that refer to the same data.
    Modification of routers through calls to non-`const` member functions
    is not thread-safe and must not be performed concurrently with any
    other member function.

    @par Constraints
    `Req` must be publicly derived from @ref basic_request.
    `Res` must be publicly derived from @ref basic_response.

    @tparam Req The type of request object.
    @tparam Res The type of response object.
*/
template<class Req, class Res>
class basic_router : public detail::any_router
{
    // Req must be publicly derived from basic_request
    BOOST_CORE_STATIC_ASSERT(
        detail::derived_from<basic_request, Req>::value);

    // Res must be publicly derived from basic_response
    BOOST_CORE_STATIC_ASSERT(
        detail::derived_from<basic_response, Res>::value);

    template<class T>
    using is_handler = detail::get_handler_kind<T, Req, Res>;

public:
    /** The type of request object used in handlers

        Route handlers and error handlers must have these
        invocable signatures respectively:
        @code
        route_result(Req&, Res&)
        route_result(Req&, Res&, system::error_code)
        @endcode
    */
    using request_type = Req;

    /** The type of response object used in handlers

        Route handlers and error handlers must have these
        invocable signatures respectively:
        @code
        route_result(Req&, Res&)
        route_result(Req&, Res&, system::error_code)
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

    /** Construct a router from another router with compatible types.

        This constructs a router that shares the same underlying routing
        state as another router whose request and response types are base
        classes of `Req` and `Res`, respectively.

        The resulting router participates in shared ownership of the
        implementation; copying the router does not duplicate routes or
        handlers, and changes visible through one router are visible
        through all routers that share the same underlying state.

        @par Constraints
        `Req` must be derived from `OtherReq`, and `Res` must be
        derived from `OtherRes`.

        @tparam OtherReq The request type of the source router.
        @tparam OtherRes The response type of the source router.
        @param other The router to copy.
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

    /** Add one or more middleware handlers for a path prefix.

        Each handler registered with this function participates in the
        routing and error-dispatch process for requests whose path begins
        with the specified prefix, as described in the @ref basic_router
        class documentation. Handlers execute in the order they are added
        and may return @ref route::next to transfer control to the
        subsequent handler in the chain.

        The prefix match is not strict: middleware attached to `"/api"`
        will also match `"/api/users"` and `"/api/data"`. When registered
        before route handlers for the same prefix, middleware runs before
        those routes. This is analogous to `app.use(path, ...)` in
        Express.js.

        @par Example
        @code
        router.use("/api",
            [](Request& req, Response& res)
            {
                if (!authenticate(req))
                {
                    res.status(401);
                    res.set_body("Unauthorized");
                    return route::done;
                }
                return route::next;
            },
            [](Request&, Response& res)
            {
                res.set_header("X-Powered-By", "MyServer");
                return route::next;
            });
        @endcode

        @par Preconditions
        @p `pattern` must be a valid path prefix; it may be empty to
            indicate the root scope.

        @param pattern The pattern to match.
        @param h1 The first handler to install.
        @param hn Additional handlers to install.
    */
    // VFALCO constrain that all are valid handlers
    template<class H1, class... HN>
    void use(
        core::string_view pattern,
        H1&& h1, HN... hn)
    {
        add_impl(pattern,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
    }

    /** Add one or more global middleware handlers.

        Each handler registered with this function participates in the
        routing and error-dispatch process as described in the class
        documentation for @ref basic_router. Handlers execute in the
        order they were added, and may return @ref route::next to transfer
        control to the subsequent handler in the chain.

        This is equivalent to writing:
        @code
        use( "/", h1, hn... );
        @endcode

        @par Example
        @code
        router.use(
            [](Request&, Response& res)
            {
                res.m.erase( "X-Powered-By" );
                return route::next;
            });
        @endcode

        @par Constraints
        `h1` is a valid handler type

        @param h1 The first middleware handler.
        @param hn Additional middleware handlers.
    */
    template<class H1, class... HN
        // VFALCO constrain that all are valid handlers
        , class = typename std::enable_if<is_handler<H1>::value != 0>::type
    >
    void use(H1&& h1, HN&&... hn)
    {
        use(core::string_view(),
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    /** Return a fluent route for the specified path pattern.

        Adds a new route to the router for the given pattern.
        A new route object is always created, even if another
        route with the same pattern already exists. The returned
        @ref fluent_route reference allows method-specific handler
        registration (such as GET or POST) or catch-all handlers
        with @ref fluent_route::all.

        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @return A fluent route interface for chaining handler registrations.
    */
    auto
    route(
        core::string_view pattern) -> fluent_route
    {
        return fluent_route(*this, pattern);
    }

    /** Register handlers for all HTTP methods matching a path pattern.
    
        Creates a new route for the given pattern and appends one or more
        handlers that run when the route matches, regardless of HTTP method.
        A new route object is created even if the pattern already exists.
        Handlers execute in registration order. Returns a @ref fluent_route
        for chaining.
    
        @code
        router.route("/status")
            .head(check_headers)
            .get(send_status)
            .all(log_access);
        @endcode
    
        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers, invoked after @p h1 in registration order.
        @return A @ref fluent_route for additional registrations.
    */
    // VFALCO constrain that all are handlers and not error handlers
    template<class H1, class... HN>
    auto all(
        core::string_view pattern,
        H1&& h1, HN&&... hn) -> fluent_route
    {
        return this->route(pattern).all(
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }

    /** Register handlers for a specific HTTP method and path pattern.

        Creates a new route for the specified pattern and attaches
        one or more handlers that will be invoked for incoming requests
        whose targets match that pattern and whose method matches the
        specified value. A new route object is always created, even if
        another route with the same pattern already exists. Handlers are
        executed in the order of registration.

        @param verb The HTTP method to match.
        @param pattern The path expression to match against request
        targets. This may include parameters or wildcards following
        the router's pattern syntax.
        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after @p h1 in registration order.
    */
    // VFALCO constrain that all are handlers and not error handlers
    template<class H1, class... HN>
    void add(
        http_proto::method verb,
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        this->route(pattern).add(
            verb,
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }
    template<class H1, class... HN>
    void add(
        core::string_view verb,
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        this->route(pattern).add(
            verb,
            std::forward<H1>(h1),
            std::forward<HN>(hn)...);
    }


    //--------------------------------------------

    auto
    dispatch(
        http_proto::method verb,
        urls::url_view const& url,
        Req& req, Res& res) ->
            route_result
    {
        if(verb == http_proto::method::unknown)
            detail::throw_invalid_argument();
        return dispatch_impl(verb,
            core::string_view(), url, req, res);
    }

    auto
    dispatch(
        core::string_view verb,
        urls::url_view const& url,
        Req& req, Res& res) ->
            route_result
    {
        // verb cannot be empty
        if(verb.empty())
            detail::throw_invalid_argument();
        return dispatch_impl(
            http_proto::method::unknown,
                verb, url, req, res);
    }

    auto
    resume(
        Req& req, Res& res,
        route_result const& ec) const ->
            route_result
    {
        return resume_impl(req, res, ec);
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
        add_impl(e, verb, handlers);
    }
};

//-----------------------------------------------

template<class Req, class Res>
class basic_router<Req, Res>::
    fluent_route
{
public:
    fluent_route(fluent_route const&) = default;

    /** Register handlers that apply to all HTTP methods.

        Adds one or more handlers that will be invoked for any request
        whose method is not matched by a more specific registration.
        These handlers run in the order of registration relative to
        other method-specific handlers on the same route.

        This function returns a @ref fluent_route, allowing
        additional method registrations to be chained.
        For example:
        @code
        router.route("/resource")
            .all(log_request)
            .get(show_resource)
            .post(update_resource);
        @endcode

        @param h1 The first handler to invoke when the route matches.
        @param hn Additional handlers, invoked sequentially after
        @p h1 in registration order.
        @return The fluent route interface for further
        chained handler registrations.
    */
    template<class H1, class... HN>
    auto all(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        owner_.add_impl(e_, core::string_view(),
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
        return *this;
    }

    /** Register handlers for a specific HTTP method.

        Adds one or more handlers to be invoked when a request matches
        this route and uses the specified HTTP method. The method must
        not be @ref http_proto::method::unknown; attempting to register
        handlers with that value will throw an exception.

        Handlers are executed in the order of registration relative to
        other handlers for the same route and method. The function
        returns the @ref fluent_route, allowing
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
        @return The fluent route interface for further
            chained handler registrations.
        @ref http_proto::method::unknown.
    */
    template<class H1, class... HN>
    auto add(
        http_proto::method verb,
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        owner_.add_impl(e_, verb,
            make_handler_list(
                std::forward<H1>(h1),
                std::forward<HN>(hn)...));
        return *this;
    }

    template<class H1, class... HN>
    auto add(
        core::string_view verb,
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        // all methods if verb.empty()==true
        owner_.add_impl(e_, verb,
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
        : e_(owner.new_layer(pattern))
        , owner_(owner)
    {
    }

    layer& e_;
    basic_router& owner_;
};

//-----------------------------------------------

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
        return 1;
    }

    std::size_t count(
        std::integral_constant<int, 3>) const noexcept
    {
        return 1 + h.count();
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

    // (Req&, Res&, error_code)
    route_result
    invoke(Req& req, Res& res,
        std::integral_constant<int, 2>) const
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

    // any_router
    route_result invoke(Req& req, Res& res,
        std::integral_constant<int, 3>) const
    {
        auto const& ec = static_cast<
            basic_response const&>(res).ec_;
        if(! ec.failed())
            return h.dispatch_impl(req, res);
        return beast2::route::next;
    }
};

} // beast2
} // boost

#endif
