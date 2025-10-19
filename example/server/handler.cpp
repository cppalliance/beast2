//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "handler.hpp"
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
namespace http_io {

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

//------------------------------------------------

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

//------------------------------------------------

// Return a reasonable mime type based on the extension of a file.
static
core::string_view
get_extension(
    core::string_view path) noexcept
{
    auto const pos = path.rfind(".");
    if( pos == core::string_view::npos)
        return core::string_view();
    return path.substr(pos);
}

static
core::string_view
mime_type(
    core::string_view path)
{
    using urls::grammar::ci_is_equal;
    auto ext = get_extension(path);
    if(ci_is_equal(ext, ".htm"))  return "text/html";
    if(ci_is_equal(ext, ".html")) return "text/html";
    if(ci_is_equal(ext, ".php"))  return "text/html";
    if(ci_is_equal(ext, ".css"))  return "text/css";
    if(ci_is_equal(ext, ".txt"))  return "text/plain";
    if(ci_is_equal(ext, ".js"))   return "application/javascript";
    if(ci_is_equal(ext, ".json")) return "application/json";
    if(ci_is_equal(ext, ".xml"))  return "application/xml";
    if(ci_is_equal(ext, ".swf"))  return "application/x-shockwave-flash";
    if(ci_is_equal(ext, ".flv"))  return "video/x-flv";
    if(ci_is_equal(ext, ".png"))  return "image/png";
    if(ci_is_equal(ext, ".jpe"))  return "image/jpeg";
    if(ci_is_equal(ext, ".jpeg")) return "image/jpeg";
    if(ci_is_equal(ext, ".jpg"))  return "image/jpeg";
    if(ci_is_equal(ext, ".gif"))  return "image/gif";
    if(ci_is_equal(ext, ".bmp"))  return "image/bmp";
    if(ci_is_equal(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(ci_is_equal(ext, ".tiff")) return "image/tiff";
    if(ci_is_equal(ext, ".tif"))  return "image/tiff";
    if(ci_is_equal(ext, ".svg"))  return "image/svg+xml";
    if(ci_is_equal(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
static
void
path_cat(
    std::string& result,
    core::string_view base,
    core::string_view path)
{
    if(base.empty())
    {
        result = path;
        return;
    }
    result = base;

#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
}

//------------------------------------------------

static
void
make_error_response(
    http_proto::status code,
    http_proto::request_base const& req,
    http_proto::response& res,
    http_proto::serializer& sr)
{
    std::string body;
    prepare_error(res, body, code, req);
    res.set_payload_size(body.size());
#if 0
    auto rv = urls::parse_authority(
        req.value_or(http_proto::field::host, ""));
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
    if(code == http_proto::status::not_found)
    {
        s += "<p>The requested URL ";
        s += req.target();
        s += " was not found on this server.</p>\n";
    }
    s += "<hr>\n";
    s += "<address>Boost.Http.IO/1.0b (Win10) Server at ";
        s += rv->host_address();
        s += " Port ";
        s += rv->port();
        s += "</address>\n";
    s += "</body></html>\n";
    res.set_start_line(code, res.version());
    res.set_keep_alive(req.keep_alive());
    res.set_payload_size(s.size());
    res.append(http_proto::field::content_type,
        "text/html; charset=iso-8859-1");
    res.append(http_proto::field::date,
        "Mon, 12 Dec 2022 03:26:32 GMT");
    res.append(http_proto::field::server,
        "Boost.Http.IO/1.0b (Win10)");
#endif

    sr.start(res, http_proto::string_body(
        std::move(body)));
}

static
void
service_unavailable(
    http_proto::response& res,
    http_proto::serializer& sr,
    http_proto::request_base const& req)
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

//------------------------------------------------

void
file_responder::
operator()(
    handler_params& params) const
{
    if(params.is_shutting_down)
        return service_unavailable(
            params.res, params.serializer, params.req);

#if 0
    // Returns a server error response
    auto const server_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };
#endif

    // Request path must be absolute and not contain "..".
    if( params.req.target().empty() ||
        params.req.target()[0] != '/' ||
        params.req.target().find("..") != core::string_view::npos)
        return make_error_response(http_proto::status::bad_request,
            params.req, params.res, params.serializer);

    // Build the path to the requested file
    std::string path; 
    path_cat(path, doc_root_, params.req.target());
    if(params.req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    system::error_code ec;
    http_proto::file f;
    std::uint64_t size = 0;
    f.open(path.c_str(), http_proto::file_mode::scan, ec);
    if(! ec.failed())
        size = f.size(ec);
    if(! ec.failed())
    {
        params.res.set_start_line(
            http_proto::status::ok,
            params.req.version());
        params.res.set(http_proto::field::server, "Boost");
        params.res.set_keep_alive(params.req.keep_alive());
        params.res.set_payload_size(size);

        auto mt = mime_type(get_extension(path));
        params.res.append(
            http_proto::field::content_type, mt);

        params.serializer.start<http_proto::file_source>(
            params.res, std::move(f), size);
        return;
    }

    if(ec == system::errc::no_such_file_or_directory)
        return make_error_response(
            http_proto::status::not_found,
                params.req, params.res, params.serializer);

    // ec.message()?
    return make_error_response(
        http_proto::status::internal_server_error,
            params.req, params.res, params.serializer);
}

//------------------------------------------------

void
https_redirect_responder::
operator()(handler_params& params) const
{
    if(params.is_shutting_down)
        return service_unavailable(
            params.res, params.serializer, params.req);

    std::string body;
    prepare_error(params.res, body,
        http_proto::status::moved_permanently, params.req);
    urls::url u1(params.req.target());
    u1.set_scheme_id(urls::scheme::https);
    u1.set_host_address("localhost"); // VFALCO WTF IS THIS!
    params.res.append(http_proto::field::location, u1.buffer());
    params.serializer.start(params.res,
        http_proto::string_body( std::move(body)));
}

} // http_io
} // boost

