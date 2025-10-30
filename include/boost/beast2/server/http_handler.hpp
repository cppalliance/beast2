//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_HTTP_HANDLER_HPP
#define BOOST_BEAST2_SERVER_HTTP_HANDLER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/router.hpp>
#include <boost/http_proto/request_parser.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/error_code.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace beast2 {

//------------------------------------------------

/** Function to detach a handler from its session

    This holds an reference to an implementation
    which detaches the handler from its session.
*/
class detacher
{
public:
    class resumer;

    /** Base class of the implementation
    */
    struct owner
    {
        virtual resumer do_detach() = 0;
        virtual void do_resume(system::error_code const&) = 0;
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

    resumer operator()();

private:
    owner* p_ = nullptr;
};

//------------------------------------------------

/** Function to resume a route handler's session

    This holds a reference to an implementation
    which resumes the handler's session. The resume
    function is returned by calling @ref detach.
*/
class detacher::resumer
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
        owner& who) noexcept
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
    owner* p_ = nullptr;
};

inline
auto
detacher::
operator()() ->
    resumer
{
    BOOST_ASSERT(p_);
    return p_->do_detach();
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
    http_proto::method method;
    urls::segments_encoded_view path;

    acceptor_config port;
    http_proto::request_base const& m;
    http_proto::request_parser& pr;
    bool is_shutting_down;
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

    /*
    bool send(core::string_view);
    bool error(system::error_code);
    bool status(http_proto::status);
    */
};

struct AsioResponse : Response
{
    template<class... Args>
    AsioResponse(Args&&... args)
        : Response(std::forward<Args>(args)...)
    {
    }
};

using router_type = router<Response>;

} // beast2
} // boost

#endif
