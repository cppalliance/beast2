//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_MULTIPART_FORM_HPP
#define BURL_MULTIPART_FORM_HPP

#include <boost/buffers/buffer.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/http_proto/source.hpp>
#include <boost/optional.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <vector>

namespace buffers    = boost::buffers;
namespace http_proto = boost::http_proto;
using error_code     = boost::system::error_code;

class multipart_form
{
    struct part
    {
        bool is_file = false;
        std::string name;
        std::string value;
        std::uint64_t size;
        boost::optional<std::string> filename;
        boost::optional<std::string> content_type;
        std::string headers;
    };

    // boundary with extra "--" prefix and postfix.
    std::array<char, 2 + 46 + 2> storage_;
    std::vector<part> pacapy_;

public:
    class source;

    multipart_form();

    void
    append(
        bool is_file,
        std::string name,
        std::string value,
        boost::optional<std::string> filename = {},
        boost::optional<std::string> content_type = {},
        std::vector<std::string> headers = {});

    http_proto::method
    method() const noexcept;

    std::string
    content_type() const;

    std::uint64_t
    content_length() const noexcept;

    source
    body() const;
};

class multipart_form::source : public http_proto::source
{
    const multipart_form* form_;
    std::vector<part>::const_iterator it_{ form_->pacapy_.begin() };
    int step_           = 0;
    std::uint64_t skip_ = 0;

public:
    explicit source(const multipart_form* form) noexcept;

    results
    on_read(buffers::mutable_buffer mb) override;
};

#endif
