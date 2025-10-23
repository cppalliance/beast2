//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/http_handler.hpp>
#include <boost/http_proto/string_body.hpp>

namespace boost {
namespace beast2 {

Response&
Response::
status(
    http_proto::status code)
{
    m.set_start_line(code, m.version());
    return *this;
}

Response&
Response::
set_body(std::string s)
{
    if(! m.exists(http_proto::field::content_type))
    {
        // VFALCO TODO detect Content-Type
        m.set(http_proto::field::content_type,
            "text/plain; charset=UTF-8");
    }

    if(!m.exists(http_proto::field::content_length))
    {
        m.set_payload_size(s.size());
    }

    sr.start(m, http_proto::string_body(std::string(s)));

    return *this;
}

} // beast2
} // boost
