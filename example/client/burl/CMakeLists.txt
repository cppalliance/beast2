#
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/beast2
#

if(NOT cxx_std_20 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    return()
endif()

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS *.cpp *.hpp
    CMakeLists.txt
    Jamfile)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${PFILES})

add_executable(beast2_example_client_burl ${PFILES})

target_compile_definitions(beast2_example_client_burl
    PRIVATE BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET beast2_example_client_burl
    PROPERTY FOLDER "examples")

find_package(OpenSSL REQUIRED)

target_compile_features(beast2_example_client_burl PUBLIC cxx_std_20)

target_link_libraries(beast2_example_client_burl
    Boost::beast2
    Boost::url
    Boost::program_options
    Boost::scope
    OpenSSL::SSL
    OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(beast2_example_client_burl crypt32)
endif()

if (TARGET Boost::capy_zlib)
    target_link_libraries(beast2_example_client_burl Boost::capy_zlib)
endif()

if (TARGET Boost::capy_brotli)
    target_link_libraries(beast2_example_client_burl Boost::capy_brotli)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Libpsl)
if (Libpsl_FOUND)
    target_link_libraries(beast2_example_client_burl Libpsl::Libpsl)
    target_compile_definitions(beast2_example_client_burl PRIVATE BURL_HAS_LIBPSL)
endif ()
