//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_DETAIL_CONFIG_HPP
#define BOOST_BEAST2_DETAIL_CONFIG_HPP

#include <boost/config.hpp>

namespace boost {
namespace beast2 {

# if (defined(BOOST_BEAST2_DYN_LINK) || defined(BOOST_ALL_DYN_LINK)) && !defined(BOOST_BEAST2_STATIC_LINK)
#  if defined(BOOST_BEAST2_SOURCE)
#   define BOOST_BEAST2_DECL        BOOST_SYMBOL_EXPORT
#   define BOOST_BEAST2_CLASS_DECL  BOOST_SYMBOL_EXPORT
#   define BOOST_BEAST2_BUILD_DLL
#  else
#   define BOOST_BEAST2_DECL        BOOST_SYMBOL_IMPORT
#   define BOOST_BEAST2_CLASS_DECL  BOOST_SYMBOL_IMPORT
#  endif
# endif // shared lib
# ifndef  BOOST_BEAST2_DECL
#  define BOOST_BEAST2_DECL
# endif
# if !defined(BOOST_BEAST2_SOURCE) && !defined(BOOST_ALL_NO_LIB) && !defined(BOOST_BEAST2_NO_LIB)
#  define BOOST_LIB_NAME boost_json
#  if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_BEAST2_DYN_LINK)
#   define BOOST_DYN_LINK
#  endif
#  include <boost/config/auto_link.hpp>
# endif

//------------------------------------------------

#if defined(__cpp_lib_coroutine) && __cpp_lib_coroutine >= 201902L
# define BOOST_BEAST2_HAS_CORO 1
#elif defined(__cpp_impl_coroutine) && __cpp_impl_coroutines >= 201902L
# define BOOST_BEAST2_HAS_CORO 1
#endif

//------------------------------------------------

// Add source location to error codes
#ifdef BOOST_BEAST2_NO_SOURCE_LOCATION
# define BOOST_BEAST2_ERR(ev) (::boost::system::error_code(ev))
# define BOOST_BEAST2_RETURN_EC(ev) return (ev)
#else
# define BOOST_BEAST2_ERR(ev) ( \
    ::boost::system::error_code( (ev), [] { \
    static constexpr auto loc((BOOST_CURRENT_LOCATION)); \
    return &loc; }()))
# define BOOST_BEAST2_RETURN_EC(ev)                                  \
    do {                                                                 \
        static constexpr auto loc ## __LINE__((BOOST_CURRENT_LOCATION)); \
        return ::boost::system::error_code((ev), &loc ## __LINE__);      \
    } while(0)
#endif

} // beast2

namespace http_proto {}
namespace beast2 {
namespace http = http_proto;
}

} // boost

#endif
