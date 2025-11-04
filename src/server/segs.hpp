//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_SEGS_HPP
#define BOOST_BEAST2_SERVER_SEGS_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace beast2 {

class split_range
{
public:
    class iterator;

    iterator begin() const noexcept;
    iterator end() const noexcept;

private:
    core::string_view s_;
};

auto
split(
    core::string_view s,
    char sep) ->
        split_range
{
}

} // beast2
} // boost

#endif
