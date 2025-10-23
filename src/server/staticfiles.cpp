//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/staticfiles.hpp>
#include <boost/http_proto/file_source.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <string>

#if 0
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/url/url.hpp>
#include <boost/url/authority_view.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>
#endif

namespace boost {
namespace beast2 {

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
    core::string_view prefix,
    urls::segments_view suffix)
{
    result = prefix;

#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
#else
    char constexpr path_separator = '/';
#endif
    if( result.back() == path_separator)
        result.resize(result.size() - 1); // remove trailing
#ifdef BOOST_MSVC
    for(auto& c : result)
        if( c == '/')
            c = path_separator;
#endif
    for(auto const& seg : suffix)
    {
        result.push_back(path_separator);
        result.append(seg);
    }
}

//------------------------------------------------

struct staticfiles::impl
{
    // server& srv;
    std::string path;
};

staticfiles::
~staticfiles() = default;

staticfiles::
staticfiles(staticfiles&&) noexcept = default;

staticfiles::
staticfiles(
    core::string_view path)
    : impl_(new impl)
{
    impl_->path = std::string(path);
}

bool
staticfiles::
operator()(
    Request& req,
    Response& res) const
{
    // Request path must be absolute and not contain "..".
#if 0
    if( req.req.target().empty() ||
        req.req.target()[0] != '/' ||
        req.req.target().find("..") != core::string_view::npos)
    {
        make_error_response(http_proto::status::bad_request,
            req.req, res.res, res.sr);
        return true;
    }
#endif

    // Build the path to the requested file
    std::string path;
    path_cat(path, impl_->path, req.path);
    if(req.pr.get().target().back() == '/')
    {
        path.push_back('/');
        path.append("index.html");
    }

    // Attempt to open the file
    system::error_code ec;
    http_proto::file f;
    std::uint64_t size = 0;
    f.open(path.c_str(), http_proto::file_mode::scan, ec);
    if(! ec.failed())
        size = f.size(ec);
    if(! ec.failed())
    {
        res.res.set_start_line(
            http_proto::status::ok,
            req.req.version());
        res.res.set_payload_size(size);

        auto mt = mime_type(get_extension(path));
        res.res.append(
            http_proto::field::content_type, mt);

        res.sr.start<http_proto::file_source>(
            res.res, std::move(f), size);
        return true;
    }

    if(ec == system::errc::no_such_file_or_directory)
        return false;

    // VFALCO TODO perform next(err)
    return false;
}

} // beast2
} // boost

