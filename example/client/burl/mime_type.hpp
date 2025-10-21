//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_MIME_TYPE_HPP
#define BURL_MIME_TYPE_HPP

#include <boost/core/detail/string_view.hpp>

namespace core = boost::core;

core::string_view
mime_type(core::string_view path) noexcept;

#endif
