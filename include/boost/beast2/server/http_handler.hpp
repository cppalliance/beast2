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

namespace boost {
namespace beast2 {

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
