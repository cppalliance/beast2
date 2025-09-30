//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "worker.hpp"
#include <boost/http_proto/string_body.hpp>
#include <boost/url/authority_view.hpp>

namespace boost {
namespace http_io {

void
service_unavailable(
    http_proto::request_base const& req,
    http_proto::response& res,
    http_proto::serializer& sr)
{
    auto const code = http_proto::status::service_unavailable;
    auto rv = urls::parse_authority( req.value_or( http_proto::field::host, "" ) );
    core::string_view host;
    if(rv.has_value())
        host = rv->buffer();
    else
        host = "";

    std::string s;
    s  = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n";
    s += "<html><head>\n";
    s += "<title>";
        s += std::to_string(static_cast<
            std::underlying_type<
                http_proto::status>::type>(code));
        s += " ";
        s += http_proto::obsolete_reason(code);
        s += "</title>\n";
    s += "</head><body>\n";
    s += "<h1>";
        s += http_proto::obsolete_reason(code);
        s += "</h1>\n";
    s += "<hr>\n";
    s += "<address>Boost.Http.IO/1.0b (Win10) Server at ";
        s += rv->host_address();
        s += " Port ";
        s += rv->port();
        s += "</address>\n";
    s += "</body></html>\n";

    res.set_start_line(code, res.version());
    res.set_keep_alive(false);
    res.set_payload_size(s.size());
    res.append(http_proto::field::content_type, "text/html; charset=iso-8859-1");
    //res.append(http_proto::field::date, "Mon, 12 Dec 2022 03:26:32 GMT" );
    res.append(http_proto::field::server, "Boost.Http.Io");

    sr.start(
        res,
        http_proto::string_body(
            std::move(s)));
}

} // http_io
} // boost
