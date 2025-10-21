//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_MESSAGE_HPP
#define BURL_MESSAGE_HPP

#include "multipart_form.hpp"

#include <boost/http_proto/file_source.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/serializer.hpp>

#include <variant>

namespace core = boost::core;

class string_body
{
    std::string body_;
    std::string content_type_;

public:
    string_body(std::string body, std::string content_type);

    http_proto::method
    method() const noexcept;

    core::string_view
    content_type() const noexcept;

    std::size_t
    content_length() const noexcept;

    buffers::const_buffer
    body() const noexcept;
};

class file_body
{
    std::string path_;

public:
    file_body(std::string path);

    http_proto::method
    method() const noexcept;

    core::string_view
    content_type() const noexcept;

    std::uint64_t
    content_length() const;

    http_proto::file_source
    body() const;
};

class stdin_body
{
public:
    class source : public http_proto::source
    {
    public:
        results
        on_read(buffers::mutable_buffer mb) override;
    };

    http_proto::method
    method() const noexcept;

    core::string_view
    content_type() const noexcept;

    boost::optional<std::size_t>
    content_length() const noexcept;

    source
    body() const;
};

class message
{
    std::variant<
        std::monostate,
        string_body,
        multipart_form,
        file_body,
        stdin_body>
        body_;

public:
    message() = default;

    template<
        class Body,
        std::enable_if_t<!std::is_same_v<std::decay_t<Body>, message>, int> = 0>
    message(Body&& body)
        : body_{ std::move(body) }
    {
    }

    void
    set_headers(http_proto::request& request) const;

    void
    start_serializer(
        http_proto::serializer& serializer,
        http_proto::request& request) const;
};

#endif
