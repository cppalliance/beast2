//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "file.hpp"

#include <boost/http_proto/file.hpp>
#include <boost/system/system_error.hpp>

namespace http_proto = boost::http_proto;
using system_error   = boost::system::system_error;

std::uint64_t
filesize(const std::string& path)
{
    http_proto::file file;
    boost::system::error_code ec;

    file.open(path.c_str(), http_proto::file_mode::scan, ec);
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
