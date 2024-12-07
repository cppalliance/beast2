//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_UTILS_HPP
#define BURL_UTILS_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/optional/optional_fwd.hpp>

namespace core = boost::core;

boost::optional<std::string>
extract_filename_form_content_disposition(core::string_view sv) noexcept;

#endif
