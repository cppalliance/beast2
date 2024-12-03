//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "multipart_form.hpp"

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

std::uint64_t
filesize(core::string_view path)
{
    http_proto::file file;
    boost::system::error_code ec;

    file.open(std::string{ path }.c_str(), http_proto::file_mode::scan, ec);
    if(ec)
        throw system_error{ ec };

    const auto size = file.size(ec);
    if(ec)
        throw system_error{ ec };

    return size;
}

core::string_view
filename(core::string_view path) noexcept
{
    const auto pos = path.find_last_of("/\\");
    if((pos != core::string_view::npos))
        return path.substr(pos + 1);
    return path;
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

        rs += part.size;

        rs += 4; // <CRLF><CRLF> after header
        rs += 2; // <CRLF> after content
    }
    rs += storage_.size(); // --boundary--
    return rs;
}

void
multipart_form::append_text(
    std::string name,
    std::string value,
    boost::optional<std::string> content_type)
{
    auto size = value.size();
    parts_.push_back(
        { std::move(name),
          boost::none,
          std::move(content_type),
          size,
          std::move(value) });
}

void
multipart_form::append_file(
    std::string name,
    std::string path,
    boost::optional<std::string> content_type)
{
    auto filename = ::filename(path);
    auto size     = ::filesize(path);
    parts_.push_back(
        { std::move(name),
          std::string{ filename },
          std::move(content_type),
          size,
          std::move(path) });
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
            ++step_;
        case 1:
            if(!copy(content_disposition_))
                return rs;
            ++step_;
        case 2:
            if(!copy(it_->name))
                return rs;
            ++step_;
        case 3:
            if(!copy("\""))
                return rs;
            ++step_;
        case 4:
            if(!it_->filename.has_value())
                goto content_type;
            if(!copy(filename_))
                return rs;
            ++step_;
        case 5:
            if(!copy(it_->filename.value()))
                return rs;
            ++step_;
        case 6:
            if(!copy("\""))
                return rs;
            ++step_;
        case 7:
        content_type:
            if(!it_->content_type.has_value())
                goto end_of_header;
            if(!copy(content_type_))
                return rs;
            ++step_;
        case 8:
            if(!copy(it_->content_type.value()))
                return rs;
            ++step_;
        case 9:
        end_of_header:
            if(!copy("\r\n\r\n"))
                return rs;
            ++step_;
        case 10:
        {
            if(it_->filename.has_value()) // file
            {
                if(!read(it_->value_or_path, it_->size))
                    return rs;
            }
            else // text
            {
                if(!copy(it_->value_or_path))
                    return rs;
            }
        }
            ++step_;
        case 11:
            if(!copy("\r\n"))
                return rs;
            step_ = 0;
            ++it_;
        }
    }

    // --boundary--
    if(!copy({ form_->storage_.data(), form_->storage_.size() }))
        return rs;

    rs.finished = true;
    return rs;
}
