//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/route_handler.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/assert.hpp>
#include <cstring>

namespace boost {
namespace beast2 {

#if 0
route_result
Response::
fail(system::error_code const& ec)
{
    if(! ec.failed())
        detail::throw_invalid_argument();
    return ec;
}
#endif

Response&
Response::
status(
    http_proto::status code)
{
    message.set_start_line(code, message.version());
    return *this;
}

Response&
Response::
set_body(std::string s)
{
    if(! message.exists(http_proto::field::content_type))
    {
        // VFALCO TODO detect Content-Type
        message.set(http_proto::field::content_type,
            "text/plain; charset=UTF-8");
    }

    if(!message.exists(http_proto::field::content_length))
    {
        message.set_payload_size(s.size());
    }

    serializer.start(message,
        http_proto::string_body(std::string(s)));

    return *this;
}

} // beast2
} // boost
