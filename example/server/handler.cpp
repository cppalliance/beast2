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
#include <boost/url/authority_view.hpp>
#include <boost/url/grammar/ci_string.hpp>

// Return a reasonable mime type based on the extension of a file.
boost::core::string_view
get_extension(
    boost::core::string_view path) noexcept
{
    auto const pos = path.rfind(".");
    if( pos == boost::core::string_view::npos)
        return boost::core::string_view();
    return path.substr(pos);
}

boost::core::string_view
mime_type(
    boost::core::string_view path)
{
    using boost::urls::grammar::ci_is_equal;
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
void
path_cat(
    std::string& result,
    boost::core::string_view base,
    boost::core::string_view path)
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

void
make_error_response(
    boost::http_proto::status code,
    boost::http_proto::request_base const& req,
    boost::http_proto::response& res,
    boost::http_proto::serializer& sr)
{
    auto rv = boost::urls::parse_authority(
        req.value_or(boost::http_proto::field::host, ""));
    boost::core::string_view host;
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
                boost::http_proto::status>::type>(code));
        s += " ";
        s += boost::http_proto::obsolete_reason(code);
        s += "</title>\n";
    s += "</head><body>\n";
    s += "<h1>";
        s += boost::http_proto::obsolete_reason(code);
        s += "</h1>\n";
    if(code == boost::http_proto::status::not_found)
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
    res.append(boost::http_proto::field::content_type,
        "text/html; charset=iso-8859-1");
    res.append(boost::http_proto::field::date,
        "Mon, 12 Dec 2022 03:26:32 GMT");
    res.append(boost::http_proto::field::server,
        "Boost.Http.IO/1.0b (Win10)");

    sr.start(
        res,
        boost::http_proto::string_body(
            std::move(s)));
}

//------------------------------------------------

void
handle_request(
    boost::core::string_view doc_root,
    boost::http_proto::request_base const& req,
    boost::http_proto::response& res,
    boost::http_proto::serializer& sr)
{
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
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != boost::core::string_view::npos)
        return make_error_response(
            boost::http_proto::status::bad_request, req, res, sr);

    // Build the path to the requested file
    std::string path; 
    path_cat(path, doc_root, req.target());
    if(req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    boost::system::error_code ec;
    boost::http_proto::file f;
    std::uint64_t size = 0;
    f.open(path.c_str(), boost::http_proto::file_mode::scan, ec);
    if(! ec.failed())
        size = f.size(ec);
    if(! ec.failed())
    {
        res.set_start_line(
            boost::http_proto::status::ok,
            req.version());
        res.set(boost::http_proto::field::server, "Boost");
        res.set_keep_alive(req.keep_alive());
        res.set_payload_size(size);

        auto mt = mime_type(get_extension(path));
        res.append(
            boost::http_proto::field::content_type, mt);

        sr.start<boost::http_proto::file_source>(
            res, std::move(f), size);
        return;
    }

    // ec.message()?
    return make_error_response(
        boost::http_proto::status::internal_server_error,
            req, res, sr);
}
