//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_DETAIL_EXCEPT_HPP
#define BOOST_BEAST2_DETAIL_EXCEPT_HPP

#include <boost/assert/source_location.hpp>
#include <boost/beast2/detail/config.hpp>

namespace boost {
namespace beast2 {
namespace detail {

BOOST_BEAST2_DECL void BOOST_NORETURN throw_logic_error(
    source_location const& loc = BOOST_CURRENT_LOCATION);

BOOST_BEAST2_DECL void BOOST_NORETURN throw_invalid_argument(
    source_location const& loc = BOOST_CURRENT_LOCATION);

} // detail
} // beast2
} // boost

#endif
