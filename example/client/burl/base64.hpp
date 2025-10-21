//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_BASE64_HPP
#define BURL_BASE64_HPP

#include <boost/core/detail/string_view.hpp>

#include <string>

namespace core = boost::core;

void
base64_encode(std::string& dest, core::string_view src);

#endif
