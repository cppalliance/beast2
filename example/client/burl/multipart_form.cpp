//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "multipart_form.hpp"
#include "file.hpp"

#include <boost/buffers/algorithm.hpp>
#include <boost/buffers/buffer_copy.hpp>
#include <boost/http_proto/file.hpp>
#include <boost/system/system_error.hpp>

#include <random>

namespace core     = boost::core;
using system_error = boost::system::system_error;

namespace
{
std::array<char, 2 + 46 + 2>
generate_boundary()
{
    std::array<char, 2 + 46 + 2> rs;
    constexpr static char chars[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    std::uniform_int_distribution<int> dist{ 0, sizeof(chars) - 2 };
    std::fill(rs.begin(), rs.end(), '-');
    std::generate(
        rs.begin() + 2 + 24, rs.end() - 2, [&] { return chars[dist(rd)]; });
    return rs;
}

std::string
serialize_headers(std::vector<std::string>& headers)
{
    std::string rs;
    for(const auto& h : headers)
    {
        rs.append("\r\n");
        rs.append(h);
    }
    return rs;
}

core::string_view content_disposition_ =
    "\r\nContent-Disposition: form-data; name=\"";
core::string_view filename_     = "; filename=\"";
core::string_view content_type_ = "\r\nContent-Type: ";
} // namespace

// -----------------------------------------------------------------------------

multipart_form::multipart_form()
    : storage_{ generate_boundary() }
{
}

void
multipart_form::append(
    bool is_file,
    std::string name,
    std::string value,
    boost::optional<std::string> filename,
    boost::optional<std::string> content_type,
    std::vector<std::string> headers)
{
    auto size = is_file ? ::filesize(value) : value.size();

    parts_.push_back(
        { is_file,
          std::move(name),
          std::move(value),
          size,
          std::move(filename),
          std::move(content_type),
          serialize_headers(headers) });
}

http_proto::method
multipart_form::method() const
{
    return http_proto::method::post;
}

std::string
multipart_form::content_type() const
{
    std::string res = "multipart/form-data; boundary=";
    // append boundary
    res.append(storage_.begin() + 2, storage_.end() - 2);
    return res;
}

std::uint64_t
multipart_form::content_length() const noexcept
{
    auto rs = std::uint64_t{};
    for(const auto& part : parts_)
    {
        rs += storage_.size() - 2; // --boundary
        rs += content_disposition_.size();
        rs += part.name.size();
        rs += 1; // closing double quote

        if(part.content_type)
        {
            rs += content_type_.size();
            rs += part.content_type->size();
        }

        if(part.filename)
        {
            rs += filename_.size();
            rs += part.filename->size();
            rs += 1; // closing double quote
        }

        rs += part.headers.size();

        rs += 4; // <CRLF><CRLF> after headers
        rs += part.size;
        rs += 2; // <CRLF> after content
    }
    rs += storage_.size(); // --boundary--
    rs += 2;               // <CRLF>
    return rs;
}

multipart_form::source
multipart_form::body() const
{
    return source{ this };
}

// -----------------------------------------------------------------------------

multipart_form::source::source(const multipart_form* form) noexcept
    : form_{ form }
{
}

multipart_form::source::results
multipart_form::source::on_read(buffers::mutable_buffer mb)
{
    auto rs = results{};

    auto copy = [&](core::string_view sv)
    {
        auto copied = buffers::buffer_copy(
            mb,
            buffers::sans_prefix(
                buffers::const_buffer{ sv.data(), sv.size() },
                static_cast<std::size_t>(skip_)));

        mb = buffers::sans_prefix(mb, copied);
        rs.bytes += copied;
        skip_ += copied;

        if(skip_ != sv.size())
            return false;

        skip_ = 0;
        return true;
    };

    auto read = [&](const std::string& path, uint64_t size)
    {
        http_proto::file file;

        file.open(path.c_str(), http_proto::file_mode::read, rs.ec);
        if(rs.ec)
            return false;

        file.seek(skip_, rs.ec);
        if(rs.ec)
            return false;

        auto read = file.read(
            mb.data(),
            (std::min)(static_cast<std::uint64_t>(mb.size()), size),
            rs.ec);
        if(rs.ec)
            return false;

        mb = buffers::sans_prefix(mb, read);
        rs.bytes += read;
        skip_ += read;

        if(skip_ != size)
            return false;

        skip_ = 0;
        return true;
    };

    while(it_ != form_->parts_.end())
    {
        switch(step_)
        {
        case 0:
            // --boundary
            if(!copy({ form_->storage_.data(), form_->storage_.size() - 2 }))
                return rs;
            step_ = 1;
            BOOST_FALLTHROUGH;
        case 1:
            if(!copy(content_disposition_))
                return rs;
            step_ = 2;
            BOOST_FALLTHROUGH;
        case 2:
            if(!copy(it_->name))
                return rs;
            step_ = 3;
            BOOST_FALLTHROUGH;
        case 3:
            if(!copy("\""))
                return rs;
            step_ = 4;
            BOOST_FALLTHROUGH;
        case 4:
            if(!it_->filename)
                goto content_type;
            if(!copy(filename_))
                return rs;
            step_ = 5;
            BOOST_FALLTHROUGH;
        case 5:
            if(!copy(it_->filename.value()))
                return rs;
            step_ = 6;
            BOOST_FALLTHROUGH;
        case 6:
            if(!copy("\""))
                return rs;
            step_ = 7;
            BOOST_FALLTHROUGH;
        case 7:
        content_type:
            if(!it_->content_type)
                goto headers;
            if(!copy(content_type_))
                return rs;
            step_ = 8;
            BOOST_FALLTHROUGH;
        case 8:
            if(!copy(it_->content_type.value()))
                return rs;
            step_ = 9;
            BOOST_FALLTHROUGH;
        case 9:
        headers:
            if(!copy(it_->headers))
                return rs;
            step_ = 10;
            BOOST_FALLTHROUGH;
        case 10:
            if(!copy("\r\n\r\n"))
                return rs;
            step_ = 11;
            BOOST_FALLTHROUGH;
        case 11:
            if(it_->is_file)
            {
                if(!read(it_->value, it_->size))
                    return rs;
            }
            else
            {
                if(!copy(it_->value))
                    return rs;
            }
            step_ = 12;
            BOOST_FALLTHROUGH;
        case 12:
            if(!copy("\r\n"))
                return rs;
            ++it_;
            step_ = 0;
        }
    }

    switch(step_)
    {
    case 0:
        // --boundary--
        if(!copy({ form_->storage_.data(), form_->storage_.size() }))
            return rs;
        step_ = 1;
        BOOST_FALLTHROUGH;
    case 1:
        if(!copy("\r\n"))
            return rs;
    }

    rs.finished = true;
    return rs;
}
