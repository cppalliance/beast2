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
#include <boost/beast2/server/router_types.hpp>
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
struct Response : basic_response
{
    http_proto::response& m;
    http_proto::serializer& sr;

    detacher detach;

    /*
    bool send(core::string_view);
    bool error(system::error_code);
    bool status(http_proto::status);
    */

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

    route_result next() const noexcept;
    route_result next(system::error_code const& ec ) const;

    //--------------------------------------------

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
