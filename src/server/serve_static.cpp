//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/serve_static.hpp>
#include <boost/beast2/error.hpp>
#include <boost/http_proto/file_source.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <string>

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

#if 0
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
#endif

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
static
void
path_cat(
    std::string& result,
    core::string_view prefix,
    core::string_view suffix)
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
    for(auto const& c : suffix)
    {
        if(c == '/')
            result.push_back(path_separator);
        else 
            result.push_back(c);
    }
}

//------------------------------------------------

// serve-static
//
// https://www.npmjs.com/package/serve-static

struct serve_static::impl
{
    impl(
        core::string_view path_,
        options const& opt_)
        : path(path_)
        , opt(opt_)
    {
    }

    std::string path;
    options opt;
};

serve_static::
~serve_static()
{
    if(impl_)
        delete impl_;
}

serve_static::
serve_static(serve_static&& other) noexcept
    : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

serve_static::
serve_static(
    core::string_view path,
    options const& opt)
    : impl_(new impl(path, opt))
{
}

auto
serve_static::
operator()(
    http_proto::Request& req,
    http_proto::Response& res) const ->
        http_proto::route_result
{
    // Allow: GET, HEAD
    if( req.message.method() != http_proto::method::get &&
        req.message.method() != http_proto::method::head)
    {
        if(impl_->opt.fallthrough)
            return http_proto::route::next;

        res.message.set_status(
            http_proto::status::method_not_allowed);
        res.message.set(http_proto::field::allow, "GET, HEAD");
        res.set_body("");
        return http_proto::route::send;
    }

    // Build the path to the requested file
    std::string path;
    path_cat(path, impl_->path, req.path);
    if(req.parser.get().target().back() == '/')
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
        res.message.set_start_line(
            http_proto::status::ok,
            req.message.version());
        res.message.set_payload_size(size);

        auto mt = mime_type(get_extension(path));
        res.message.append(
            http_proto::field::content_type, mt);

        // send file
        res.serializer.start<http_proto::file_source>(
            res.message, std::move(f), size);
        return http_proto::route::send;
    }

    if( ec == system::errc::no_such_file_or_directory &&
        ! impl_->opt.fallthrough)
        return http_proto::route::next;

    BOOST_ASSERT(ec.failed());
    return ec;
}

} // beast2
} // boost

