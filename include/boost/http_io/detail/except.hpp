//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/CPPAlliance/http_io
//

#ifndef BOOST_HTTP_IO_DETAIL_EXCEPT_HPP
#define BOOST_HTTP_IO_DETAIL_EXCEPT_HPP

#include <boost/assert/source_location.hpp>
#include <boost/http_io/detail/config.hpp>

namespace boost {
namespace http_io {
namespace detail {

BOOST_HTTP_IO_DECL void BOOST_NORETURN throw_logic_error(
    source_location const& loc = BOOST_CURRENT_LOCATION);

} // detail
} // http_io
} // boost

#endif
