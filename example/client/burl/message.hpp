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

#include <boost/http/request.hpp>
#include <boost/http/serializer.hpp>

#include <variant>

namespace core = boost::core;

class string_body
{
    std::string body_;
    std::string content_type_;

public:
    string_body(std::string body, std::string content_type);

    http::method
    method() const noexcept;

    core::string_view
    content_type() const noexcept;

    std::size_t
    content_length() const noexcept;

    capy::const_buffer
    body() const noexcept;
};

class message
{
    std::variant<
        std::monostate,
        string_body>
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
    set_headers(http::request& request) const;

    void
    start_serializer(
        http::serializer& serializer,
        http::request& request) const;
};

#endif
