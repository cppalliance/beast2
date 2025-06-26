#
# Copyright (c) 2024 Mohammad Nejati
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

add_executable(http_io_example_client_visit ${PFILES})

target_compile_definitions(http_io_example_client_visit
    PRIVATE BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET http_io_example_client_visit
    PROPERTY FOLDER "examples")

find_package(OpenSSL REQUIRED)
find_package(ZLIB)

target_link_libraries(http_io_example_client_visit
    boost_http_io
    OpenSSL::SSL
    OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(http_io_example_client_visit
        crypt32)
endif()

if (ZLIB_FOUND)
    target_link_libraries(http_io_example_client_visit
        boost_http_proto_zlib)
endif()
