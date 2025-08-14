#
# Copyright (c) 2025 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/http_io
#

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS *.cpp *.hpp
    CMakeLists.txt
    Jamfile)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${PFILES})

add_executable(http_io_example_client_get ${PFILES})

target_compile_definitions(http_io_example_client_get
    PRIVATE BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET http_io_example_client_get
    PROPERTY FOLDER "examples")

find_package(OpenSSL REQUIRED)

target_link_libraries(http_io_example_client_get
    boost_http_io
    boost_url
    OpenSSL::SSL
    OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(http_io_example_client_get crypt32)
endif()

if (TARGET boost_rts_zlib)
    target_link_libraries(http_io_example_client_get boost_rts_zlib)
endif()

if (TARGET boost_rts_brotli)
    target_link_libraries(http_io_example_client_get boost_rts_brotli)
endif()
