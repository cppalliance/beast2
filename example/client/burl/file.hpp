//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_FILE_HPP
#define BURL_FILE_HPP

#include <boost/core/detail/string_view.hpp>

#include <cstdint>

namespace core = boost::core;

std::uint64_t
filesize(const std::string& path);

core::string_view
filename(core::string_view path) noexcept;

#endif
