//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/serve_redirect.hpp>
#include <boost/http_proto/file_source.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/url/url.hpp>
#include <boost/url/authority_view.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <iostream>

namespace boost {
namespace beast2 {

//------------------------------------------------

/// Returns the current system time formatted as an HTTP-date per RFC 9110 §5.6.7.
/// Example: "Sat, 11 Oct 2025 02:12:34 GMT"
static
std::string
make_http_date()
{
    using namespace std;

    // Get current time in UTC
    std::time_t t = std::time(nullptr);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif

    char const* wkday[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    char const* month[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    // Format strictly according to RFC 9110 (fixed-width, English locale)
    char buf[40];
    std::snprintf(
        buf, sizeof(buf),
        "%s, %02d %s %04d %02d:%02d:%02d GMT",
        wkday[tm_utc.tm_wday],
        tm_utc.tm_mday,
        month[tm_utc.tm_mon],
        tm_utc.tm_year + 1900,
        tm_utc.tm_hour,
        tm_utc.tm_min,
        tm_utc.tm_sec);

    return std::string(buf);
}

static
void
prepare_error(
    http_proto::response& res,
    std::string& body,
    http_proto::status code,
    http_proto::request_base const& req)
{
    res.set_start_line(code, req.version());
    res.append(http_proto::field::server, "boost");
    res.append(http_proto::field::date, make_http_date());
    res.append(http_proto::field::cache_control, "no-store");
    res.append(http_proto::field::content_type, "text/html");
    res.append(http_proto::field::content_language, "en");

    // format the numeric code followed by the reason string
    auto title = std::to_string(
        static_cast<std::underlying_type<
            http_proto::status>::type>(code));
    title.push_back(' ');
    title.append( res.reason() );

    std::ostringstream ss;
    ss <<
        "<HTML>"
        "<HEAD>"
        "<TITLE>" << title << "</TITLE>"
        "</HEAD>\n"
        "<BODY>"
        "<H1>" << title << "</H1>"
        "</BODY>"
        "</HTML>"
        ;
    body = ss.str();
}

auto
serve_redirect::
operator()(
    http_proto::Request& req,
    http_proto::Response& res) const ->
        http_proto::route_result
{
    std::string body;
    prepare_error(res.message, body,
        http_proto::status::moved_permanently, req.message);
    urls::url u1(req.message.target());
    u1.set_scheme_id(urls::scheme::https);
    u1.set_host_address("localhost"); // VFALCO WTF IS THIS!
    res.message.append(http_proto::field::location, u1.buffer());
    res.serializer.start(res.message,
        http_proto::string_body( std::move(body)));
    return http_proto::route::send;
}

} // beast2
} // boost

