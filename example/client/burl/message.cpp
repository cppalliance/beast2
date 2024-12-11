//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "message.hpp"
#include "file.hpp"
#include "mime_type.hpp"

#include <boost/http_proto/field.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>

using system_error = boost::system::system_error;

string_body::string_body(std::string body, std::string content_type)
    : body_{ std::move(body) }
    , content_type_{ std::move(content_type) }
{
}

http_proto::method
string_body::method() const
{
    return http_proto::method::post;
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

buffers::const_buffer
string_body::body() const noexcept
{
    return { body_.data(), body_.size() };
}

// -----------------------------------------------------------------------------

file_body::file_body(std::string path)
    : path_{ std::move(path) }
{
}

http_proto::method
file_body::method() const
{
    return http_proto::method::put;
}

core::string_view
file_body::content_type() const noexcept
{
    return mime_type(path_);
}

std::uint64_t
file_body::content_length() const
{
    return ::filesize(path_);
}

http_proto::file_body
file_body::body() const
{
    http_proto::file file;
    error_code ec;
    file.open(path_.c_str(), http_proto::file_mode::read, ec);
    if(ec)
        throw system_error{ ec };

    return http_proto::file_body{ std::move(file), content_length() };
}

// -----------------------------------------------------------------------------

boost::http_proto::source::results
stdin_body::source::on_read(buffers::mutable_buffer mb)
{
    std::cin.read(static_cast<char*>(mb.data()), mb.size());

    return { .ec       = {},
             .bytes    = static_cast<std::size_t>(std::cin.gcount()),
             .finished = std::cin.eof() };
}

http_proto::method
stdin_body::method() const
{
    return http_proto::method::put;
}

core::string_view
stdin_body::content_type() const noexcept
{
    return "application/octet-stream";
}

boost::optional<std::size_t>
stdin_body::content_length() const
{
    return boost::none;
}

stdin_body::source
stdin_body::body() const
{
    return {};
}

// -----------------------------------------------------------------------------

void
message::set_headers(http_proto::request& request) const
{
    std::visit(
        [&](auto& f)
        {
            using field = http_proto::field;
            if constexpr(!std::is_same_v<decltype(f), const std::monostate&>)
            {
                request.set_method(f.method());

                boost::optional<std::size_t> content_length =
                    f.content_length();
                if(content_length.has_value())
                {
                    request.set_content_length(content_length.value());
                    if(content_length.value() >= 1024 * 1024 &&
                       request.version() == http_proto::version::http_1_1)
                        request.set(field::expect, "100-continue");
                }
                else
                {
                    request.set_chunked(true);
                    request.set(field::expect, "100-continue");
                }

                request.set(field::content_type, f.content_type());
            }
        },
        body_);
}

void
message::start_serializer(
    http_proto::serializer& serializer,
    http_proto::request& request) const
{
    std::visit(
        [&](auto& f)
        {
            if constexpr(!std::is_same_v<decltype(f), const std::monostate&>)
            {
                serializer.start<std::decay_t<decltype(f.body())>>(
                    request, f.body());
            }
            else
            {
                serializer.start(request);
            }
        },
        body_);
}
