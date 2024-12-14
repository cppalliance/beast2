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
#include <boost/optional/optional.hpp>
#include <boost/system/result.hpp>
#include <boost/url/url.hpp>

#include <cstdint>
#include <vector>

namespace core = boost::core;
namespace urls = boost::urls;

boost::optional<std::string>
extract_filename_form_content_disposition(core::string_view sv);

struct form_option_result
{
    std::string name;
    char prefix = '\0';
    std::string value;
    boost::optional<std::string> filename;
    boost::optional<std::string> type;
    std::vector<std::string> headers;
};

form_option_result
parse_form_option(core::string_view sv);

boost::system::result<std::uint64_t>
parse_human_readable_size(core::string_view sv);

boost::system::result<urls::url>
normalize_and_parse_url(std::string str);

#endif
