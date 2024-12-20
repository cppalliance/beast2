//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_GLOB_HPP
#define BURL_GLOB_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/optional/optional_fwd.hpp>

#include <functional>
#include <string>

namespace core = boost::core;

struct glob_result
{
    std::string result;
    std::vector<std::string> tokens;

    std::string
    interpolate(core::string_view format);
};

std::function<boost::optional<glob_result>()>
make_glob_generator(core::string_view pattern);

#endif
