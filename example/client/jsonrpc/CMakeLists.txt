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

file(GLOB_RECURSE LIBFILES CONFIGURE_DEPENDS jsonrpc/*.cpp jsonrpc/*.hpp Jamfile)
add_library(http_io_example_client_jsonrpc_lib STATIC ${LIBFILES})

target_compile_definitions(http_io_example_client_jsonrpc_lib
    PUBLIC BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET http_io_example_client_jsonrpc_lib
    PROPERTY FOLDER "examples")

find_package(OpenSSL REQUIRED)

target_link_libraries(http_io_example_client_jsonrpc_lib
    PUBLIC
    boost_http_io
    boost_url
    boost_json
    boost_multiprecision
    OpenSSL::SSL
    OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(http_io_example_client_jsonrpc_lib PUBLIC crypt32)
endif()

# CPP11 Example
add_executable(http_io_example_client_jsonrpc_cpp11 cpp11.cpp eth_methods.hpp Jamfile)
set_property(TARGET http_io_example_client_jsonrpc_cpp11
    PROPERTY FOLDER "examples")
target_link_libraries(http_io_example_client_jsonrpc_cpp11
    PRIVATE
    http_io_example_client_jsonrpc_lib)
if (TARGET boost_rts_zlib)
    target_link_libraries(http_io_example_client_jsonrpc_cpp11 PRIVATE boost_rts_zlib)
endif()
if (TARGET boost_rts_brotli)
    target_link_libraries(http_io_example_client_jsonrpc_cpp11 PRIVATE boost_rts_brotli)
endif()

# CPP20 Example
if (CMAKE_CXX_STANDARD EQUAL 20)
    add_executable(http_io_example_client_jsonrpc_cpp20 cpp20.cpp eth_methods.hpp Jamfile)
    set_property(TARGET http_io_example_client_jsonrpc_cpp20
        PROPERTY FOLDER "examples")
    target_link_libraries(http_io_example_client_jsonrpc_cpp20
        PRIVATE
        http_io_example_client_jsonrpc_lib)
    if (TARGET boost_rts_zlib)
        target_link_libraries(http_io_example_client_jsonrpc_cpp20 PRIVATE boost_rts_zlib)
    endif()
    if (TARGET boost_rts_brotli)
        target_link_libraries(http_io_example_client_jsonrpc_cpp20 PRIVATE boost_rts_brotli)
    endif()
endif ()
