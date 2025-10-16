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

#include <boost/config.hpp>

namespace boost {
namespace http_io {

# if (defined(BOOST_HTTP_IO_DYN_LINK) || defined(BOOST_ALL_DYN_LINK)) && !defined(BOOST_HTTP_IO_STATIC_LINK)
#  if defined(BOOST_HTTP_IO_SOURCE)
#   define BOOST_HTTP_IO_DECL        BOOST_SYMBOL_EXPORT
#   define BOOST_HTTP_IO_CLASS_DECL  BOOST_SYMBOL_EXPORT
#   define BOOST_HTTP_IO_BUILD_DLL
#  else
#   define BOOST_HTTP_IO_DECL        BOOST_SYMBOL_IMPORT
#   define BOOST_HTTP_IO_CLASS_DECL  BOOST_SYMBOL_IMPORT
#  endif
# endif // shared lib
# ifndef  BOOST_HTTP_IO_DECL
#  define BOOST_HTTP_IO_DECL
# endif
# if !defined(BOOST_HTTP_IO_SOURCE) && !defined(BOOST_ALL_NO_LIB) && !defined(BOOST_HTTP_IO_NO_LIB)
#  define BOOST_LIB_NAME boost_json
#  if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_HTTP_IO_DYN_LINK)
#   define BOOST_DYN_LINK
#  endif
#  include <boost/config/auto_link.hpp>
# endif

} // http_io
} // boost

#endif
