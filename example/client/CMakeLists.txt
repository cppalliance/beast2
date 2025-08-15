#
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/http_io
#

find_package(OpenSSL)

add_subdirectory(visit)

if (OPENSSL_FOUND)
    add_subdirectory(burl)
    add_subdirectory(get)
    add_subdirectory(jsonrpc)
endif ()
