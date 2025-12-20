#
# Copyright (c) 2025 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/beast2
#

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS *.cpp *.hpp
    CMakeLists.txt
    Jamfile)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${PFILES})

file(GLOB_RECURSE LIBFILES CONFIGURE_DEPENDS jsonrpc/*.cpp jsonrpc/*.hpp Jamfile)
add_library(beast2_example_client_jsonrpc_lib STATIC ${LIBFILES})

target_compile_definitions(beast2_example_client_jsonrpc_lib
    PUBLIC BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET beast2_example_client_jsonrpc_lib
    PROPERTY FOLDER "examples")

find_package(OpenSSL REQUIRED)

target_link_libraries(beast2_example_client_jsonrpc_lib
    PUBLIC
    Boost::beast2
    Boost::url
    Boost::json
    Boost::multiprecision
    OpenSSL::SSL
    OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(beast2_example_client_jsonrpc_lib PUBLIC crypt32)
endif()

# CPP11 Example
add_executable(beast2_example_client_jsonrpc_cpp11 cpp11.cpp eth_methods.hpp Jamfile)
set_property(TARGET beast2_example_client_jsonrpc_cpp11
    PROPERTY FOLDER "examples")
target_link_libraries(beast2_example_client_jsonrpc_cpp11
    PRIVATE
    beast2_example_client_jsonrpc_lib)
if (TARGET Boost::capy_zlib)
    target_link_libraries(beast2_example_client_jsonrpc_cpp11 PRIVATE Boost::capy_zlib)
endif()
if (TARGET Boost::capy_brotli)
    target_link_libraries(beast2_example_client_jsonrpc_cpp11 PRIVATE Boost::capy_brotli)
endif()

# CPP20 Example
add_executable(beast2_example_client_jsonrpc_cpp20 cpp20.cpp eth_methods.hpp Jamfile)
set_property(TARGET beast2_example_client_jsonrpc_cpp20
    PROPERTY FOLDER "examples")
target_compile_features(beast2_example_client_jsonrpc_cpp20 PUBLIC cxx_std_20)
target_link_libraries(beast2_example_client_jsonrpc_cpp20
    PRIVATE
    beast2_example_client_jsonrpc_lib)
if (TARGET Boost::capy_zlib)
    target_link_libraries(beast2_example_client_jsonrpc_cpp20 PRIVATE Boost::capy_zlib)
endif()
if (TARGET Boost::capy_brotli)
    target_link_libraries(beast2_example_client_jsonrpc_cpp20 PRIVATE Boost::capy_brotli)
endif()
