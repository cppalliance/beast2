//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <boost/http_io/detail/config.hpp>
#include <boost/http_io/detail/except.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace boost {
namespace http_io {
namespace detail {

void
throw_logic_error(
    source_location const& loc)
{
    throw_exception(
        std::logic_error(
            "logic error"),
        loc);
}

} // detail
} // http_io
} // boost
