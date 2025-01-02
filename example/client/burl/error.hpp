//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_ERROR_HPP
#define BURL_ERROR_HPP

#include <boost/system/error_code.hpp>

enum class error
{
    binary_output_to_tty = 1,
};

namespace boost
{
namespace system
{
template<>
struct is_error_code_enum<error> : std::true_type
{
};
} // namespace system
} // namespace boost

std::error_code
make_error_code(error e);

#endif
