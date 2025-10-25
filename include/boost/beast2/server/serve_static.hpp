//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_SERVE_STATIC_HPP
#define BOOST_BEAST2_SERVER_SERVE_STATIC_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/http_handler.hpp>
#include <memory>

namespace boost {
namespace beast2 {

/** A route handler which serves static files from a document root

    This handler operates similary to the npm package serve-static.
*/
struct serve_static
{
    /** Policy for handling dotfiles
    */
    enum class dotfiles_policy
    {
        allow,
        deny,
        ignore
    };

    /** Options for the static file server
    */
    struct options
    {
        /** How to handle dotfiles

            Dotfiles are files or directories whose names begin with a dot (.)
            The default is to ignore dotfiles.
        */
        dotfiles_policy dotfiles = dotfiles_policy::ignore;

        // VFALCO extensions fallbacks vector

        // VFALCO vector of index file names

        /** Maximum cache age in milliseconds
        */
        // VFALCO
        // std::chrono::duration<std::chrono::milliseconds> max_age = std::chrono::milliseconds(0); ?
        std::size_t max_age_ms = 0;

        // VFALCO set_headers callback

        /** Enable accepting range requests.

            When this is false, the "Accept-Ranges" field will not be
            sent, and any "Range" field in the request will be ignored.
        */
        bool accept_ranges = true;

        /** Enable sending cache-control headers.

            When this is set to `false`, the @ref immutable
            and @ref max_age options are ignored.
        */
        bool cache_control = true;

        /** Enable etag header generation.
        */
        bool etag = true;

        /** Treat client errors as unhandled requests.

            When this value is `true`, all error codes will be
            treated as if unhandled. Otherwise, errors (including
            file not found) will go through the error handling routes.

            Typically true is desired such that multiple physical
            directories can be mapped to the same web address or for
            routes to fill in non-existent files.

            The value false can be used if this handler is mounted
            at a path that is designed to be strictly a single file system
            directory, which allows for short-circuiting 404s for less
            overhead. This handler will also reply to all methods.

            @note This handler replies to all HTTP methods.
        */
        bool fallthrough = true;

        /** Enable the immutable directive in cache control headers.

            When this is true, the "immutable" directive will be
            added to the "Cache-Control" field.
            This indicates to clients that the resource will not
            change during its freshness lifetime.
            This is typically used when the filenames contain
            a hash of the content, such as when using a build
            tool which fingerprints static assets.
            The @ref max_age value must also be set to a non-zero value.
        */
        bool immutable = false;

        /** Enable a default index file for directory requests.
            When a request is made for a directory path, such as
            "/docs/", the file "index.html" will be served if it
            exists within that directory.
        */
        bool index = true; // "index.html" default

        /** Enable the "Last-Modified" header.

            The file system's last modified value is used.
        */
        bool last_modified = true;

        /** Enable redirection for directories missing a trailing slash.

            When a request is made for a directory path without a trailing
            slash, the client is redirected to the same path with the slash
            appended. This is useful for relative links to work correctly
            in browsers.
            For example, a request for `/docs` when `/docs/index.html` exists
            will be redirected to `/docs/`.
            @note This requires that the client accepts redirections.
        */
        bool redirect = true;
    };

    BOOST_BEAST2_DECL
    ~serve_static();

    /** Constructor
        @param path The document root path
        @param options The options to use
    */
    BOOST_BEAST2_DECL
    serve_static(
        core::string_view path,
        options const& opt);

    /** Constructor
        @param path The document root path
    */
    explicit
    serve_static(
        core::string_view path)
        : serve_static(path, options{})
    {
    }

    /** Constructor
    */
    BOOST_BEAST2_DECL
    serve_static(serve_static&&) noexcept;

    /** Handle a request
        @param req The request
        @param res The response
        @return `true` if the request was handled, `false` to
        indicate the request was not handled.
    */
    BOOST_BEAST2_DECL
    system::error_code operator()(Request&, Response&) const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // beast2
} // boost

#endif
