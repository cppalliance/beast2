//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_BUFFER_HPP
#define BOOST_BEAST2_BUFFER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/capy/buffers.hpp>
#include <cstdlib>
#include <iterator>

namespace boost {
namespace beast2 {

// Re-export buffer types from capy
using capy::mutable_buffer;
using capy::const_buffer;
using capy::buffer_size;

} // beast2
} // boost

#endif
