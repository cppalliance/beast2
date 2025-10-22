//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "mime_type.hpp"

#include <boost/url/grammar/ci_string.hpp>

namespace grammar = boost::urls::grammar;

core::string_view
mime_type(core::string_view path) noexcept
{
    const auto ext = [&path]
    {
        const auto pos = path.rfind(".");
        if(pos == core::string_view::npos)
            return core::string_view{};
        return path.substr(pos);
    }();

    // clang-format off
    if(grammar::ci_is_equal(ext, ".gif"))  return "image/gif";
    if(grammar::ci_is_equal(ext, ".jpg"))  return "image/jpeg";
    if(grammar::ci_is_equal(ext, ".jpeg")) return "image/jpeg";
    if(grammar::ci_is_equal(ext, ".png"))  return "image/png";
    if(grammar::ci_is_equal(ext, ".svg"))  return "image/svg+xml";
    if(grammar::ci_is_equal(ext, ".txt"))  return "text/plain";
    if(grammar::ci_is_equal(ext, ".htm"))  return "text/html";
    if(grammar::ci_is_equal(ext, ".html")) return "text/html";
    if(grammar::ci_is_equal(ext, ".pdf"))  return "application/pdf";
    if(grammar::ci_is_equal(ext, ".xml"))  return "application/xml";
    // clang-format on

    return "application/octet-stream";
}
