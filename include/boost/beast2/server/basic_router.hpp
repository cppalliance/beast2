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

template<class, class>
class basic_router;

/** Configuration options for routers.
*/
struct router_options
{
    /** Constructor

        Routers constructed with default options inherit the values of
        @ref case_sensitive and @ref strict from the parent router.
        If there is no parent, both default to `false`.
        The value of @ref merge_params always defaults to `false`
        and is never inherited.
    */
    router_options() = default;

    /** Set whether to merge parameters from parent routers.

        This setting controls whether route parameters defined on parent
        routers are made available in nested routers. It is not inherited
        and always defaults to `false`.

        @par Example
        @code
        router r(router_options()
            .merge_params(true)
            .case_sensitive(true)
            .strict(false));
        @endcode

        @param value `true` to merge parameters from parent routers.
        @return A reference to `*this` for chaining.
    */
    router_options&
    merge_params(
        bool value) noexcept
    {
        v_ = (v_ & ~1) | (value ? 1 : 0);
        return *this;
    }

    /** Set whether pattern matching is case-sensitive.

        When this option is not set explicitly, the value is inherited
        from the parent router or defaults to `false` if there is no parent.

        @par Example
        @code
        router r(router_options()
            .case_sensitive(true)
            .strict(true));
        @endcode

        @param value `true` to perform case-sensitive path matching.
        @return A reference to `*this` for chaining.
    */
    router_options&
    case_sensitive(
        bool value) noexcept
    {
        if(value)
            v_ = (v_ & ~6) | 2;
        else
            v_ = (v_ & ~6) | 4;
        return *this;
    }

    /** Set whether pattern matching is strict.

        When this option is not set explicitly, the value is inherited
        from the parent router or defaults to `false` if there is no parent.
        Strict matching treats a trailing slash as significant:
        the pattern `"/api"` matches `"/api"` but not `"/api/"`.
        When strict matching is disabled, these paths are treated
        as equivalent.

        @par Example
        @code
        router r(router_options()
            .strict(true)
            .case_sensitive(false));
        @endcode

        @param value `true` to enable strict path matching.
        @return A reference to `*this` for chaining.
    */
    router_options&
    strict(
        bool value) noexcept
    {
        if(value)
            v_ = (v_ & ~24) | 8;
        else
            v_ = (v_ & ~24) | 16;
        return *this;
    }

private:
    template<class, class> friend class basic_router;
    unsigned int v_ = 0;
};

//-----------------------------------------------

namespace detail {

class any_router;

//-----------------------------------------------

// implementation for all routers
class any_router
{
private:
    template<class, class>
    friend class beast2::basic_router;
    using opt_flags = unsigned int;

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
    BOOST_BEAST2_DECL any_router(opt_flags);
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

    The `basic_router` class provides a routing mechanism inspired by
    Express.js, adapted to idiomatic C++. It stores and dispatches route
    handlers based on HTTP method and path patterns, enabling modular
    request processing through middleware chains and method-specific routes.

    @par Core Concepts

    Like Express.js Router, `basic_router` allows you to define handlers
    for specific HTTP methods and URL patterns. However, there are key
    differences stemming from C++'s synchronous nature versus Node.js's
    asynchronous I/O model:

    @li In Express, `res.send()` initiates an asynchronous write and
        returns immediately. In `basic_router`, handlers prepare the
        response object completely and return @ref route::send, after
        which the framework sends the response.

    @li Express handlers implicitly end the request-response cycle by
        calling response methods. C++ handlers explicitly return
        @ref route_result values to control the dispatch flow.

    @li Express relies on callbacks and promises for asynchronous
        operations. `basic_router` supports detaching from the routing
        context for async operations via @ref route::detach.

    @par Router Structure and Shared Ownership

    Router objects are lightweight, shared references to their underlying
    routing state. Copies of a router obtained through construction,
    conversion, or assignment do not duplicate routes or handlers; all
    copies refer to the same data. This matches Express.js behavior where
    router instances can be shared and reused.

    @code
    router_type router;
    router.get("/users", get_users_handler);

    router_type router_copy = router; // shares same routes
    router_copy.get("/posts", get_posts_handler); // affects both routers
    @endcode

    @par Pattern Matching

    Path patterns support parameters and follow similar conventions to
    Express.js routes. Patterns undergo percent-decoding when handlers
    are mounted, so `"/x%2Fz"` is treated identically to `"/x/z"`.

    Unlike Express.js string patterns with special characters like `*`
    and `?`, `basic_router` uses simpler prefix matching for middleware
    and exact matching for routes. Future versions may expand pattern
    support.

    When pattern matching occurs, the matched portion becomes
    `req.base_path` and the remaining portion becomes `req.path`,
    analogous to Express's `req.baseUrl` and `req.path`.

    @code
    // Pattern: "/api"
    // Request: "/api/users/123"
    // During handler execution:
    //   req.base_path == "/api"
    //   req.path == "/users/123"
    @endcode

    @par Middleware with use()

    The @ref use function registers middleware handlers that run for
    requests matching a path prefix, similar to Express's `app.use()`.
    Middleware executes in registration order and can perform
    authentication, logging, header manipulation, or other cross-cutting
    concerns.

    Unlike routes, middleware uses prefix matching. A middleware mounted
    at `"/api"` will match `"/api"`, `"/api/"`, `"/api/users"`, and any
    other path beginning with `"/api"`.

    @code
    router.use("/api",
        [](Request& req, Response& res)
        {
            // Authentication middleware
            if (!is_authenticated(req))
            {
                res.status(http_proto::status::unauthorized);
                res.set_body("Authentication required");
                return route::send;
            }
            return route::next; // continue to next handler
        },
        [](Request& req, Response& res)
        {
            // Logging middleware
            log_request(req);
            return route::next;
        });
    @endcode

    Global middleware (applying to all requests) is registered with
    `use()` without a path or with `"/"`:

    @code
    router.use([](Request& req, Response& res)
    {
        res.message.set("X-Powered-By", "Beast2");
        return route::next;
    });
    @endcode

    @par Routes and HTTP Methods

    Routes match specific HTTP methods and exact paths. Unlike middleware,
    routes require complete path matches (unless configured otherwise via
    @ref router_options::strict). Routes are added via method-specific
    functions or the fluent @ref route interface.

    @code
    // Direct method registration
    router.get("/users/:id", show_user_handler);
    router.post("/users", create_user_handler);
    router.put("/users/:id", update_user_handler);
    router.delete("/users/:id", delete_user_handler);

    // Fluent interface for multiple methods on same route
    router.route("/status")
        .get([](Request& req, Response& res)
        {
            res.set_body("OK");
            return route::send;
        })
        .head([](Request& req, Response& res)
        {
            res.status(http_proto::status::ok);
            return route::send;
        });

    // Handler for all methods
    router.all("/debug", debug_handler);
    @endcode

    @par Handler Types and Signatures

    `basic_router` supports three handler types:

    @li **Regular handlers**: Process normal requests
    @code
    route_result handler( Req& req, Res& res )
    @endcode

    @li **Error handlers**: Handle error conditions
    @code
    route_result error_handler( Req& req, Res& res, system::error_code ec )
    @endcode

    @li **Nested routers**: Sub-routers for modular organization
    @code
    router.use("/api", api_router);
    @endcode

    @par Return Values and Control Flow

    Handlers control routing flow by returning @ref route_result values.
    This explicit control differs from Express.js, where calling response
    methods implicitly ends processing.

    @li @ref route::send - Response is ready; framework should send it
    @li @ref route::next - Continue to next matching handler
    @li @ref route::next_route - Skip remaining handlers in current route
    @li @ref route::complete - Response already sent externally
    @li @ref route::close - Close connection after response
    @li @ref route::detach - Transfer ownership of connection to handler

    Express.js comparison:
    @code
    // Express.js
    app.get('/users', (req, res) => {
        res.json({ users: [] }); // implicitly ends request
    });

    // basic_router equivalent
    router.get("/users", [](Request& req, Response& res)
    {
        res.status(http_proto::status::ok);
        res.set_body(R"({"users":[]})");
        return route::send; // explicitly indicate response is ready
    });
    @endcode

    The @ref route::next return value is analogous to Express's `next()`
    function:

    @code
    // Express.js
    app.use((req, res, next) => {
        console.log('logging');
        next(); // continue to next handler
    });

    // basic_router equivalent
    router.use([](Request& req, Response& res)
    {
        log("processing request");
        return route::next; // continue to next handler
    });
    @endcode

    @par Error Handling

    When a handler returns a failing error code (where
    `system::error_code::failed()` returns `true`), the router enters
    error-dispatching mode. In this mode, only error handlers are invoked,
    similar to Express.js error middleware.

    Error handlers have a three-parameter signature including the error
    code. They execute in registration order until one returns a value
    other than @ref route::next.

    @code
    router.use("/api",
        [](Request& req, Response& res)
        {
            if (!validate_request(req))
                return make_error_code(errc::invalid_argument);
            return route::next;
        });

    // Error handler (registered like middleware)
    router.use([](Request& req, Response& res, system::error_code ec)
    {
        res.status(http_proto::status::internal_server_error);
        res.set_body("Error: " + ec.message());
        return route::send;
    });
    @endcode

    Express.js comparison:
    @code
    // Express.js error middleware (4 parameters)
    app.use((err, req, res, next) => {
        res.status(500).send('Error: ' + err.message);
    });
    @endcode

    @par Execution Order

    Handlers execute in registration order, forming a chain. This matches
    Express.js behavior. Middleware registered first executes first;
    routes are evaluated in definition order.

    @code
    router.use(logging_middleware);        // runs first
    router.use("/api", auth_middleware);   // runs second for /api paths
    router.get("/api/users", get_users);   // runs third for GET /api/users
    router.use(error_handler);             // runs if errors occur
    @endcode

    @par Nested Routers

    Routers can be mounted within other routers, enabling modular
    application structure similar to Express.js Router:

    @code
    // Create a sub-router for user-related routes
    basic_router<Request, Response> users_router;
    users_router.get("/", list_users);
    users_router.post("/", create_user);
    users_router.get("/:id", show_user);

    // Mount it on the main router
    router.use("/users", users_router);

    // These patterns now work:
    // GET  /users      -> list_users
    // POST /users      -> create_user
    // GET  /users/123  -> show_user
    @endcode

    When a nested router processes a request, `req.base_path` reflects the
    cumulative mount path, similar to Express's `req.baseUrl`.

    @par Router Options

    The @ref router_options class controls matching behavior:

    @li **case_sensitive**: Whether path matching is case-sensitive
        (default: `false`, inherited from parent)
    @li **strict**: Whether trailing slashes are significant
        (default: `false`, inherited from parent)
    @li **merge_params**: Whether to inherit parent route parameters
        (default: `false`, never inherited)

    @code
    router_options opts;
    opts.case_sensitive(true).strict(true);
    basic_router<Request, Response> router(opts);

    // With strict=true:
    // Pattern "/api" matches "/api" but NOT "/api/"
    @endcode

    @par Asynchronous Operations and Detaching

    Express.js handlers can perform asynchronous I/O using callbacks,
    promises, or async/await. In `basic_router`, handlers are synchronous
    by default. For asynchronous operations, handlers can detach from the
    routing context by returning @ref route::detach, then resume later:

    @code
    router.get("/slow", [](Request& req, Response& res)
    {
        return res.detach([&req, &res](resumer resume_fn)
        {
            // Perform async operation (e.g., with Asio)
            async_database_query([resume_fn, &res](error_code ec, result r)
            {
                if (ec)
                {
                    resume_fn(ec); // resume with error
                    return;
                }
                res.set_body(r.to_json());
                resume_fn(route::send); // resume with success
            });
        });
    });
    @endcode

    After detaching, the handler must eventually call the @ref resumer
    function to continue routing. This is conceptually similar to Express's
    asynchronous model but requires explicit management.

    @par Complete Example

    @code
    #include <boost/beast2/server/basic_router.hpp>
    #include <boost/beast2/server/route_handler.hpp>

    using namespace boost::beast2;

    // Create the router
    basic_router<Request, Response> router;

    // Global middleware - runs for all requests
    router.use([](Request& req, Response& res)
    {
        log_request(req.url);
        return route::next;
    });

    // Authentication middleware for /admin routes
    router.use("/admin", [](Request& req, Response& res)
    {
        if (!check_admin_credentials(req))
        {
            res.status(http_proto::status::forbidden);
            res.set_body("Access denied");
            return route::send;
        }
        return route::next;
    });

    // Route handlers
    router.get("/", [](Request& req, Response& res)
    {
        res.status(http_proto::status::ok);
        res.set_body("Welcome!");
        return route::send;
    });

    router.get("/hello/:name", [](Request& req, Response& res)
    {
        // In future versions, req.params["name"] would be available
        res.status(http_proto::status::ok);
        res.set_body("Hello!");
        return route::send;
    });

    // Multiple handlers for one route
    router.route("/users/:id")
        .get([](Request& req, Response& res)
        {
            res.set_body(get_user_json(req));
            return route::send;
        })
        .put([](Request& req, Response& res)
        {
            update_user(req);
            res.status(http_proto::status::no_content);
            return route::send;
        })
        .delete([](Request& req, Response& res)
        {
            delete_user(req);
            res.status(http_proto::status::no_content);
            return route::send;
        });

    // Error handler - runs when handlers return failing error codes
    router.use([](Request& req, Response& res, system::error_code ec)
    {
        res.status(http_proto::status::internal_server_error);
        res.set_body("Internal error: " + ec.message());
        return route::send;
    });

    // Dispatch a request
    Request req;
    Response res;
    auto result = router.dispatch(
        http_proto::method::get,
        urls::parse_uri("/hello/world").value(),
        req, res);

    if (result == route::send)
    {
        // Send response to client
        send_response(res);
    }
    @endcode

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

    @see router_options, route_result, route, basic_request, basic_response
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

    // 0 = unrecognized
    // 1 = normal handler (Req&, Res&)
    // 2 = error handler  (Req&, Res&, error_code)
    // 4 = basic_router<Req, Res>

    template<class T, class = void>
    struct handler_type
        : std::integral_constant<int, 0>
    {
    };

    // route_result( Req&, Res& ) const
    template<class T>
    struct handler_type<T, typename
        std::enable_if<std::is_convertible<
            decltype(std::declval<T const&>()(
                std::declval<Req&>(),
                std::declval<Res&>())),
            route_result>::value
        >::type> : std::integral_constant<int, 1> {};

    // route_result( Req&, Res&, system::error_code const& ) const
    template<class T>
    struct handler_type<T, typename
        std::enable_if<std::is_convertible<
            decltype(std::declval<T const&>()(
                std::declval<Req&>(),
                std::declval<Res&>(),
                std::declval<system::error_code const&>())),
            route_result>::value
        >::type> : std::integral_constant<int, 2> {};

    // basic_router<Req, Res>
    template<class T>
    struct handler_type<T, typename
        std::enable_if<
            std::is_base_of<any_router, T>::value &&
            std::is_convertible<T const volatile*,
                any_router const volatile*>::value &&
            std::is_constructible<T, basic_router<Req, Res>>::value
        >::type> : std::integral_constant<int, 4> {};

    template<std::size_t Mask, class... Ts>
    struct handler_check : std::true_type {};

    template<std::size_t Mask, class T0, class... Ts>
    struct handler_check<Mask, T0, Ts...>
        : std::conditional<
              ( (handler_type<T0>::value & Mask) != 0 ),
              handler_check<Mask, Ts...>,
              std::false_type
          >::type {};

public:
    /** The type of request object used in handlers
    */
    using request_type = Req;

    /** The type of response object used in handlers
    */
    using response_type = Res;

    /** A fluent interface for defining handlers on a specific route.

        This type represents a single route within the router and
        provides a chainable API for registering handlers associated
        with particular HTTP methods or for all methods collectively.

        Typical usage registers one or more handlers for a route:
        @code
        router.route("/users/:id")
            .get(show_user)
            .put(update_user)
            .all(log_access);
        @endcode

        Each call appends handlers in registration order.
    */
    class fluent_route;

    /** Constructor

        Creates an empty router with the specified configuration.
        Routers constructed with default options inherit the values
        of @ref router_options::case_sensitive and
        @ref router_options::strict from the parent router, or default
        to `false` if there is no parent. The value of
        @ref router_options::merge_params defaults to `false` and
        is never inherited.

        @param options The configuration options to use.
    */
    explicit
    basic_router(router_options options = {})
        : any_router(options.v_)
    {
    }

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

        @par Example
        @code
        router.use("/api",
            [](Request& req, Response& res)
            {
                if (!authenticate(req))
                {
                    res.status(401);
                    res.set_body("Unauthorized");
                    return route::send;
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
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void use(
        core::string_view pattern,
        H1&& h1, HN... hn)
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler,
        // error handler, or router.
        BOOST_CORE_STATIC_ASSERT(handler_check<7, H1, HN...>::value);
        add_impl(pattern, make_handler_list(
            std::forward<H1>(h1), std::forward<HN>(hn)...));
    }

    /** Add one or more global middleware handlers.
        
        Each handler registered with this function participates in the
        routing and error-dispatch process as described in the
        @ref basic_router class documentation. Handlers execute in the
        order they are added and may return @ref route::next to transfer
        control to the next handler in the chain.

        This is equivalent to writing:
        @code
        use( "/", h1, hn... );
        @endcode

        @par Example
        @code
        router.use(
            [](Request&, Response& res)
            {
                res.message.erase("X-Powered-By");
                return route::next;
            });
        @endcode

        @par Constraints
        @li `h1` must not be convertible to @ref core::string_view.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN
        , class = typename std::enable_if<
            ! std::is_convertible<H1, core::string_view>::value>::type>
    void use(H1&& h1, HN&&... hn)
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler,
        // error handler, or router.
        BOOST_CORE_STATIC_ASSERT(handler_check<7, H1, HN...>::value);
        use(core::string_view(),
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add handlers for all HTTP methods matching a path pattern.
        
        This registers regular handlers for the specified path pattern,
        participating in dispatch as described in the @ref basic_router
        class documentation. Handlers run when the route matches,
        regardless of HTTP method, and execute in registration order.
        Error handlers and routers cannot be passed here. A new route
        object is created even if the pattern already exists.

        @code
        router.route("/status")
            .head(check_headers)
            .get(send_status)
            .all(log_access);
        @endcode

        @par Preconditions
        @p `pattern` must be a valid path pattern; it must not be empty.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void all(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler.
        // Error handlers and routers cannot be passed here.
        BOOST_CORE_STATIC_ASSERT(handler_check<1, H1, HN...>::value);
        this->route(pattern).all(
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more route handlers for a method and pattern.
        
        This registers regular handlers for the specified HTTP verb and
        path pattern, participating in dispatch as described in the
        @ref basic_router class documentation. Error handlers and
        routers cannot be passed here.

        @param verb The known HTTP method to match.
        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void add(
        http_proto::method verb,
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler.
        // Error handlers and routers cannot be passed here.
        BOOST_CORE_STATIC_ASSERT(handler_check<1, H1, HN...>::value);
        this->route(pattern).add(verb,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more route handlers for a method and pattern.
        
        This registers regular handlers for the specified HTTP verb and
        path pattern, participating in dispatch as described in the
        @ref basic_router class documentation. Error handlers and
        routers cannot be passed here.

        @param verb The HTTP method string to match.
        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void add(
        core::string_view verb,
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler.
        // Error handlers and routers cannot be passed here.
        BOOST_CORE_STATIC_ASSERT(handler_check<1, H1, HN...>::value);
        this->route(pattern).add(verb,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
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
        the router's pattern syntax. May not be empty.
        @return A fluent route interface for chaining handler registrations.
    */
    auto
    route(
        core::string_view pattern) -> fluent_route
    {
        return fluent_route(*this, pattern);
    }

    /** Add one or more GET route handlers for a pattern.

        This is a convenience function equivalent to calling
        `add(http_proto::method::get, pattern, h1, hn...)`.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void get(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        add(http_proto::method::get, pattern,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more POST route handlers for a pattern.

        This is a convenience function equivalent to calling
        `add(http_proto::method::post, pattern, h1, hn...)`.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void post(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        add(http_proto::method::post, pattern,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more PUT route handlers for a pattern.

        This is a convenience function equivalent to calling
        `add(http_proto::method::put, pattern, h1, hn...)`.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void put(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        add(http_proto::method::put, pattern,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more DELETE route handlers for a pattern.

        This is a convenience function equivalent to calling
        `add(http_proto::method::delete_, pattern, h1, hn...)`.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void delete_(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        add(http_proto::method::delete_, pattern,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more PATCH route handlers for a pattern.

        This is a convenience function equivalent to calling
        `add(http_proto::method::patch, pattern, h1, hn...)`.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void patch(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        add(http_proto::method::patch, pattern,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more HEAD route handlers for a pattern.

        This is a convenience function equivalent to calling
        `add(http_proto::method::head, pattern, h1, hn...)`.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void head(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        add(http_proto::method::head, pattern,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add one or more OPTIONS route handlers for a pattern.

        This is a convenience function equivalent to calling
        `add(http_proto::method::options, pattern, h1, hn...)`.

        @param pattern The path pattern to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
    */
    template<class H1, class... HN>
    void options(
        core::string_view pattern,
        H1&& h1, HN&&... hn)
    {
        add(http_proto::method::options, pattern,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    //--------------------------------------------

    /** Dispatch a request to the appropriate handler.
        
        This runs the routing and error-dispatch logic for the given HTTP
        method and target URL, as described in the @ref basic_router class
        documentation.

        @par Thread Safety
        This function may be called concurrently on the same object along
        with other `const` member functions. Each concurrent invocation
        must use distinct request and response objects.

        @param verb The HTTP method to match. This must not be
            @ref http_proto::method::unknown.
        @param url The full request target used for route matching.
        @param req The request to pass to handlers.
        @param res The response to pass to handlers.
        @return The @ref route_result describing how routing completed.
        @throws std::invalid_argument If @p verb is
            @ref http_proto::method::unknown.
    */
    auto
    dispatch(
        http_proto::method verb,
        urls::url_view const& url,
        Req& req, Res& res) const ->
            route_result
    {
        if(verb == http_proto::method::unknown)
            detail::throw_invalid_argument();
        return dispatch_impl(verb,
            core::string_view(), url, req, res);
    }

    /** Dispatch a request to the appropriate handler using a method string.
        
        This runs the routing and error-dispatch logic for the given HTTP
        method string and target URL, as described in the @ref basic_router
        class documentation. This overload is intended for method tokens
        that are not represented by @ref http_proto::method.

        @par Thread Safety
        This function may be called concurrently on the same object along
        with other `const` member functions. Each concurrent invocation
        must use distinct request and response objects.

        @param verb The HTTP method string to match. This must not be empty.
        @param url The full request target used for route matching.
        @param req The request to pass to handlers.
        @param res The response to pass to handlers.
        @return The @ref route_result describing how routing completed.
        @throws std::invalid_argument If @p verb is empty.
    */
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

    /** Resume dispatch after a detached handler.
        
        This continues routing after a previous call to @ref dispatch
        returned @ref route::detach. It recreates the routing state and
        resumes as if the handler that detached had instead returned
        the specified @p ec from its body. The regular routing and
        error-dispatch logic then proceeds as described in the
        @ref basic_router class documentation. For example, if @p ec is
        @ref route::next, the next matching handlers are invoked.

        @par Thread Safety
        This function may be called concurrently on the same object along
        with other `const` member functions. Each concurrent invocation
        must use distinct request and response objects.

        @param req The request to pass to handlers.
        @param res The response to pass to handlers.
        @param rv The @ref route_result to resume with, as if returned
            by the detached handler.
        @return The @ref route_result describing how routing completed.
    */
    auto
    resume(
        Req& req, Res& res,
        route_result const& rv) const ->
            route_result
    {
        return resume_impl(req, res, rv);
    }

private:
    // wrapper for route handlers
    template<class H>
    struct handler_impl : any_handler
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
                handler_type<decltype(h)>{});
        }

        route_result
        invoke(
            basic_request& req,
            basic_response& res) const override
        {
            return invoke(
                static_cast<Req&>(req),
                static_cast<Res&>(res),
                handler_type<decltype(h)>{});
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
            std::integral_constant<int, 4>) const noexcept
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
            std::integral_constant<int, 4>) const
        {
            auto const& ec = static_cast<
                basic_response const&>(res).ec_;
            if(! ec.failed())
                return h.dispatch_impl(req, res);
            return beast2::route::next;
        }
    };

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

    /** Add handlers that apply to all HTTP methods.
        
        This registers regular handlers that run for any request matching
        the route's pattern, regardless of HTTP method. Handlers are
        appended to the route's handler sequence and are invoked in
        registration order whenever a preceding handler returns
        @ref route::next. Error handlers and routers cannot be passed here.

        This function returns a @ref fluent_route, allowing additional
        method registrations to be chained. For example:
        @code
        router.route("/resource")
            .all(log_request)
            .get(show_resource)
            .post(update_resource);
        @endcode

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto all(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler.
        // Error handlers and routers cannot be passed here.
        BOOST_CORE_STATIC_ASSERT(handler_check<1, H1, HN...>::value);
        owner_.add_impl(e_, core::string_view(), make_handler_list(
            std::forward<H1>(h1), std::forward<HN>(hn)...));
        return *this;
    }

    /** Add handlers for a specific HTTP method.
        
        This registers regular handlers for the given method on the
        current route, participating in dispatch as described in the
        @ref basic_router class documentation. Handlers are appended
        to the route's handler sequence and invoked in registration
        order whenever a preceding handler returns @ref route::next.
        Error handlers and routers cannot be passed here.

        @param verb The HTTP method to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto add(
        http_proto::method verb,
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler.
        // Error handlers and routers cannot be passed here.
        BOOST_CORE_STATIC_ASSERT(handler_check<1, H1, HN...>::value);
        owner_.add_impl(e_, verb, make_handler_list(
            std::forward<H1>(h1), std::forward<HN>(hn)...));
        return *this;
    }

    /** Add handlers for a method name.
        
        This registers regular handlers for the given HTTP method string
        on the current route, participating in dispatch as described in
        the @ref basic_router class documentation. This overload is
        intended for methods not represented by @ref http_proto::method.
        Handlers are appended to the route's handler sequence and invoked
        in registration order whenever a preceding handler returns
        @ref route::next.

        @par Constraints
        @li Each handler must be a regular handler; error handlers and
            routers cannot be passed.

        @param verb The HTTP method string to match.
        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto add(
        core::string_view verb,
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        // If you get a compile error on this line it means that
        // one or more of the provided types is not a valid handler.
        // Error handlers and routers cannot be passed here.
        BOOST_CORE_STATIC_ASSERT(handler_check<1, H1, HN...>::value);
        owner_.add_impl(e_, verb, make_handler_list(
            std::forward<H1>(h1), std::forward<HN>(hn)...));
        return *this;
    }

    /** Add GET handlers to the route.

        This is a convenience function equivalent to calling
        `add(http_proto::method::get, h1, hn...)`.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto get(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        return add(http_proto::method::get,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add POST handlers to the route.

        This is a convenience function equivalent to calling
        `add(http_proto::method::post, h1, hn...)`.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto post(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        return add(http_proto::method::post,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add PUT handlers to the route.

        This is a convenience function equivalent to calling
        `add(http_proto::method::put, h1, hn...)`.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto put(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        return add(http_proto::method::put,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add DELETE handlers to the route.

        This is a convenience function equivalent to calling
        `add(http_proto::method::delete_, h1, hn...)`.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto delete_(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        return add(http_proto::method::delete_,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add PATCH handlers to the route.

        This is a convenience function equivalent to calling
        `add(http_proto::method::patch, h1, hn...)`.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto patch(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        return add(http_proto::method::patch,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add HEAD handlers to the route.

        This is a convenience function equivalent to calling
        `add(http_proto::method::head, h1, hn...)`.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto head(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        return add(http_proto::method::head,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
    }

    /** Add OPTIONS handlers to the route.

        This is a convenience function equivalent to calling
        `add(http_proto::method::options, h1, hn...)`.

        @param h1 The first handler to add.
        @param hn Additional handlers to add, invoked after @p h1 in
            registration order.
        @return The @ref fluent_route for further chained registrations.
    */
    template<class H1, class... HN>
    auto options(
        H1&& h1, HN&&... hn) ->
            fluent_route
    {
        return add(http_proto::method::options,
            std::forward<H1>(h1), std::forward<HN>(hn)...);
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

} // beast2
} // boost

#endif
