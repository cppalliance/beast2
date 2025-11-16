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
#include <boost/beast2/polystore.hpp>
#include <boost/beast2/server/router_types.hpp>
#include <boost/http_proto/request.hpp>  // VFALCO forward declare?
#include <boost/http_proto/request_parser.hpp>  // VFALCO forward declare?
#include <boost/http_proto/response.hpp>        // VFALCO forward declare?
#include <boost/http_proto/serializer.hpp>      // VFALCO forward declare?
#include <boost/url/url_view.hpp>
#include <boost/system/error_code.hpp>

namespace boost {
namespace beast2 {

struct acceptor_config
{
    bool is_ssl;
    bool is_admin;
};

/** Request object for HTTP route handlers
*/
struct Request : basic_request
{
    /** The complete request target

        This is the parsed directly from the start
        line contained in the HTTP request and is
        never modified.
    */
    urls::url_view url;

    /** The HTTP request message
    */
    http_proto::request message;

    /** The HTTP request parser
        This can be used to take over reading the body.
    */
    http_proto::request_parser parser;

    /** A container for storing arbitrary data associated with the request.
        This starts out empty for each new request.
    */
    polystore data;
};

//-----------------------------------------------

/** Response object for HTTP route handlers
*/
struct Response : basic_response
{
    /** The HTTP response message
    */
    http_proto::response message;

    /** The HTTP response serializer
    */
    http_proto::serializer serializer;

    /** The detacher for this session.
        This can be used to detach from the
        session and take over managing the
        connection.
    */
    detacher detach;

    /** A container for storing arbitrary data associated with the session.

        This starts out empty for each new session.
    */
    polystore data;

    route_result close() const noexcept
    {
        return route::close;
    }

    route_result complete() const noexcept
    {
        return route::complete;
    }

    route_result next() const noexcept
    {
        return route::next;
    }

    route_result next_route() const noexcept
    {
        return route::next_route;
    }

    route_result send() const noexcept
    {
        return route::send;
    }

    BOOST_BEAST2_DECL
    route_result
    fail(system::error_code const& ec);

    // route_result send(core::string_view);

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
};

} // beast2
} // boost

#endif
