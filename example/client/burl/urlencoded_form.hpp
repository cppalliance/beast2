//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_URLENCODED_FORM_HPP
#define BURL_URLENCODED_FORM_HPP

#include <boost/buffers/const_buffer.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/http_proto/method.hpp>

#include <istream>
#include <string>

namespace buffers    = boost::buffers;
namespace core       = boost::core;
namespace http_proto = boost::http_proto;

class urlencoded_form
{
    std::string body_;

public:
    void
    append(core::string_view name, core::string_view value) noexcept;

    void
    append(std::istream& is);

    http_proto::method
    method() const;

    core::string_view
    content_type() const noexcept;

    std::size_t
    content_length() const noexcept;

    buffers::const_buffer
    body() const noexcept;

private:
    void
    append_encoded(core::string_view sv);
};

#endif
