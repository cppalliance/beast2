//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTE_HANDLER_HPP
#define BOOST_BEAST2_SERVER_ROUTE_HANDLER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/http_proto/request_parser.hpp>  // VFALCO forward declare?
#include <boost/http_proto/response.hpp>        // VFALCO forward declare?
#include <boost/http_proto/serializer.hpp>      // VFALCO forward declare?
#include <boost/url/url_view.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/error_code.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace beast2 {

/** The type of value returned by route handlers
*/
using route_result = system::error_code;

/** Route handler return values

    These values determine how the caller proceeds after invoking
    a route handler. Each enumerator describes a distinct control
    action; whether the request was handled, should continue to the
    next route, transfers ownership of the session, or signals that
    the connection should be closed.
*/
enum class route
{
    /** The request was handled

        This indicates that the route handler processed
        the request and set up the response serializer.
        The calling function will write the response
        before reading the next request or closing
        the connection.
    */
    send,

    /** The route handler did not handle the request

        Indicates that the route handler declined to process
        the request. The caller will continue attempting to
        match subsequent route handlers until one returns
        `send` or no routes remain.
    */
    next,

    /** The route handler requests that the connection be closed

        Indicates that no further requests will be processed.
        The caller should close the connection once the current
        response, if any, has been sent.
    */
    close,

    /** The route handler is detaching from the session

        Indicates that ownership of the session or stream is
        being transferred to the handler. The caller will not
        perform further I/O or manage the connection after
        this return value.
    */
    detach,

    /** The route handler processed the request and sent the response

        Indicates that the handler has completed the response. The caller
        will read the next HTTP request if the connection is persistent,
        or close the connection otherwise.
    */
    done
};

} // beast2
namespace system {
template<>
struct is_error_code_enum<
    ::boost::beast2::route>
{
    static bool const value = true;
};
} // system
namespace beast2 {

namespace detail {
struct BOOST_SYMBOL_VISIBLE route_cat_type
    : system::error_category
{
    BOOST_BEAST2_DECL const char* name(
        ) const noexcept override;
    BOOST_BEAST2_DECL std::string message(
        int) const override;
    BOOST_BEAST2_DECL char const* message(
        int, char*, std::size_t
            ) const noexcept override;
    bool failed( int ) const noexcept override
         { return false; }
    BOOST_SYSTEM_CONSTEXPR route_cat_type()
        : error_category(0x51c90d393754ecdf )
    {
    }
};
BOOST_BEAST2_DECL extern route_cat_type route_cat;
} // detail

inline
BOOST_SYSTEM_CONSTEXPR
system::error_code
make_error_code(route ev) noexcept
{
    return system::error_code{static_cast<
        std::underlying_type<route>::type>(ev),
        detail::route_cat};
}

//------------------------------------------------

class resumer;

/** Function to detach a route handler from its session

    This holds an reference to an implementation
    which detaches the handler from its session.
*/
class detacher
{
public:
    /** Base class of the implementation
    */
    struct owner
    {
        virtual resumer do_detach() = 0;
        virtual void do_resume(route_result const&) = 0;
    };

    detacher() = default;
    detacher(detacher const&) = default;
    detacher& operator=(detacher const&) = default;

    explicit
    detacher(
        owner& who) noexcept
        : p_(&who)
    {
    }

    /** Detach and invoke the given function

        The function will be invoked with this equivalent signature:
        @code
        void( resumer );
        @endcode

        @return A @ref route_result equal to @ref route::detach
    */
    template<class F>
    route_result    
    operator()(F&& f);

private:
    friend resumer;
    owner* p_ = nullptr;
};

//------------------------------------------------

/** Function to resume a route handler's session

    This holds a reference to an implementation
    which resumes the handler's session. The resume
    function is returned by calling @ref detach.
*/
class resumer
{
public:
    /** Constructor

        Default constructed resume functions will
        be empty. Invoking an empty resume function
        yields undefined behavior.
    */
    resumer() = default;

    /** Constructor

        Copies of resume functions behave the same
        as the original
    */
    resumer(resumer const&) = default;

    /** Assignment

        Copies of resume functions behave the same
        as the original
    */
    resumer& operator=(resumer const&) = default;

    /** Constructor
    */
    explicit
    resumer(
        detacher::owner& who) noexcept
        : p_(&who)
    {
    }

    /** Resume the session

        A session is resumed as if the detached
        handler returned the error code in `ec`.

        @param ec The error code to resume with.
    */
    void operator()(
        system::error_code const& ec = {}) const
    {
        p_->do_resume(ec);
    }

private:
    detacher::owner* p_ = nullptr;
};

template<class F>
auto    
detacher::
operator()(F&& f) ->
    route_result
{
    if(! p_)
        detail::throw_logic_error();
    std::forward<F>(f)(p_->do_detach());
    return route::detach;
}

//------------------------------------------------

struct acceptor_config
{
    bool is_ssl;
    bool is_admin;
};

/** Request object for HTTP route handlers
*/
struct Request
{
    /** The mount path of the current router

        This is the portion of the request path
        which was matched to select the handler.
        The remaining portion is available in
        @ref path.
    */
    core::string_view base_path;

    /** The current pathname, relative to the base path
    */
    core::string_view path;

    /** The complete request target

        This is the parsed directly from the start
        line contained in the HTTP request and is
        never modified.
    */
    urls::url_view url;

    acceptor_config port;
    http_proto::request_base const& m;
    http_proto::request_parser& pr;

    Request(
        acceptor_config port_,
        http_proto::request_base const& m_,
        http_proto::request_parser& pr_)
        : port(port_)
        , m(m_)
        , pr(pr_)
    {
    }
};

/** Response object for HTTP route handlers
*/
struct Response
{
    /** Set the status code of the response.
        @par Example
        @code
        res.status(http_proto::status::not_found);
        @endcode
        @param code The status code to set.
        @return A reference to this response.
    */
    BOOST_BEAST2_DECL
    Response&
    status(http_proto::status code);

    BOOST_BEAST2_DECL
    Response&
    set_body(std::string s);

    http_proto::response& m;
    http_proto::serializer& sr;

    detacher detach;

    /*
    bool send(core::string_view);
    bool error(system::error_code);
    bool status(http_proto::status);
    */

    Response(
        http_proto::response& m_,
        http_proto::serializer& sr_) noexcept
        : m(m_)
        , sr(sr_)
    {
    }

};

} // beast2
} // boost

#endif
