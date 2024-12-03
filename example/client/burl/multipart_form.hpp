//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_MULTIPART_FORM_HPP
#define BURL_MULTIPART_FORM_HPP

#include <boost/buffers/mutable_buffer.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/http_proto/source.hpp>
#include <boost/system/error_code.hpp>
#include <boost/optional.hpp>

#include <array>
#include <vector>

namespace buffers    = boost::buffers;
namespace http_proto = boost::http_proto;
using error_code     = boost::system::error_code;

class multipart_form
{
    struct part
    {
        std::string name;
        boost::optional<std::string> filename;
        boost::optional<std::string> content_type;
        std::uint64_t size;
        std::string value_or_path;
    };

    // boundary with extra "--" prefix and postfix.
    std::array<char, 2 + 46 + 2> storage_;
    std::vector<part> parts_;

public:
    class source;

    multipart_form();

    void
    append_text(
        std::string name,
        std::string value,
        boost::optional<std::string> content_type);

    void
    append_file(
        std::string name,
        std::string path,
        boost::optional<std::string> content_type);

    std::string
    content_type() const;

    std::uint64_t
    content_length() const noexcept;
};

class multipart_form::source : public http_proto::source
{
    const multipart_form* form_;
    std::vector<part>::const_iterator it_{ form_->parts_.begin() };
    int step_           = 0;
    std::uint64_t skip_ = 0;

public:
    explicit source(const multipart_form* form) noexcept;

    results
    on_read(buffers::mutable_buffer mb) override;
};

#endif
