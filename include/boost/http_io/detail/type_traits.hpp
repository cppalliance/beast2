//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_DETAIL_CONFIG_HPP
#define BOOST_HTTP_IO_DETAIL_CONFIG_HPP

#include <type_traits>

namespace boost {
namespace http_io {
namespace detail {

template<class Base, class Derived>
using derived_from = std::integral_constant<bool,
    std::is_base_of<Base, Derived>::value &&
    std::is_convertible<
        Derived const volatile*,
        Base const volatile*>::value>;

} // detail
} // http_io
} // boost

#endif
