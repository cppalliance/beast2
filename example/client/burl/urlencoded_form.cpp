//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "urlencoded_form.hpp"

#include <boost/url/encode.hpp>
#include <boost/url/rfc/pchars.hpp>

namespace urls = boost::urls;

void
urlencoded_form::append(
    core::string_view name,
    core::string_view value) noexcept
{
    if(!body_.empty())
        body_ += '&';
    body_ += name;
    if(!value.empty())
        body_ += '=';
    append_encoded(value);
}

void
urlencoded_form::append(std::istream& is)
{
    if(!body_.empty())
        body_ += '&';

    for(;;)
    {
        char buf[64 * 1024];
        is.read(buf, sizeof(buf));
        if(is.gcount() == 0)
            break;
        append_encoded({ buf, static_cast<std::size_t>(is.gcount()) });
    }
}

http_proto::method
urlencoded_form::method() const
{
    return http_proto::method::post;
}

core::string_view
urlencoded_form::content_type() const noexcept
{
    return "application/x-www-form-urlencoded";
}

std::size_t
urlencoded_form::content_length() const noexcept
{
    return body_.size();
}

buffers::const_buffer
urlencoded_form::body() const noexcept
{
    return { body_.data(), body_.size() };
}

void
urlencoded_form::append_encoded(core::string_view sv)
{
    urls::encoding_opts opt;
    opt.space_as_plus = true;
    urls::encode(sv, urls::pchars, opt, urls::string_token::append_to(body_));
}