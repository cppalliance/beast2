//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "message.hpp"
#include "mime_type.hpp"

#include <boost/capy/file.hpp>
#include <boost/http/field.hpp>
#include <boost/system/system_error.hpp>

#include <filesystem>
#include <iostream>

namespace capy     = boost::capy;
namespace fs       = std::filesystem;
using system_error = boost::system::system_error;

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

file_body::file_body(std::string path)
    : path_{ std::move(path) }
{
}

http::method
file_body::method() const noexcept
{
    return http::method::put;
}

core::string_view
file_body::content_type() const noexcept
{
    return mime_type(path_);
}

std::uint64_t
file_body::content_length() const
{
    return fs::file_size(path_);
}

http::file_source
file_body::body() const
{
    boost::capy::file file;
    error_code ec;
    file.open(path_.c_str(), boost::capy::file_mode::read, ec);
    if(ec)
        throw system_error{ ec };

    return http::file_source{ std::move(file), content_length() };
}

// -----------------------------------------------------------------------------

boost::http::source::results
stdin_body::source::on_read(capy::mutable_buffer mb)
{
    std::cin.read(static_cast<char*>(mb.data()), mb.size());

    return { .ec       = {},
             .bytes    = static_cast<std::size_t>(std::cin.gcount()),
             .finished = std::cin.eof() };
}

http::method
stdin_body::method() const noexcept
{
    return http::method::put;
}

core::string_view
stdin_body::content_type() const noexcept
{
    return "application/octet-stream";
}

boost::optional<std::size_t>
stdin_body::content_length() const noexcept
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

                boost::optional<std::size_t> content_length =
                    f.content_length();
                if(content_length.has_value())
                {
                    request.set_content_length(content_length.value());
                    if(content_length.value() >= 1024 * 1024 &&
                       request.version() == http::version::http_1_1)
                        request.set(field::expect, "100-continue");
                }
                else
                {
                    request.set_chunked(true);
                    request.set(field::expect, "100-continue");
                }
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
