//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <typeinfo>

namespace boost {
namespace beast2 {
namespace detail {

void
throw_bad_typeid(
    source_location const& loc)
{
    throw_exception(std::bad_typeid(), loc);
}

void
throw_invalid_argument(
    core::string_view s,
    source_location const& loc)
{
    throw_exception(std::invalid_argument(s), loc);
}

void
throw_logic_error(
    core::string_view s,
    source_location const& loc)
{
    throw_exception(std::logic_error(s), loc);
}

} // detail
} // beast2
} // boost
