//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "message.hpp"

#include <boost/http/field.hpp>

namespace capy = boost::capy;

string_body::string_body(std::string body, std::string content_type)
    : body_{ std::move(body) }
    , content_type_{ std::move(content_type) }
{
}

http::method
string_body::method() const noexcept
{
    return http::method::post;
}

core::string_view
string_body::content_type() const noexcept
{
    return content_type_;
}

std::size_t
string_body::content_length() const noexcept
{
    return body_.size();
}

capy::const_buffer
string_body::body() const noexcept
{
    return { body_.data(), body_.size() };
}

// -----------------------------------------------------------------------------

void
message::set_headers(http::request& request) const
{
    std::visit(
        [&](auto& f)
        {
            using field = http::field;
            if constexpr(!std::is_same_v<decltype(f), const std::monostate&>)
            {
                request.set_method(f.method());
                request.set(field::content_type, f.content_type());

                std::size_t content_length = f.content_length();
                request.set_content_length(content_length);
                if(content_length >= 1024 * 1024 &&
                   request.version() == http::version::http_1_1)
                    request.set(field::expect, "100-continue");
            }
        },
        body_);
}

void
message::start_serializer(
    http::serializer& serializer,
    http::request& request) const
{
    std::visit(
        [&](auto& f)
        {
            if constexpr(!std::is_same_v<decltype(f), const std::monostate&>)
            {
                serializer.start(request, f.body());
            }
            else
            {
                serializer.start(request);
            }
        },
        body_);
}
