#
# Copyright (c) 2023 Vinnie Falco (vinnie dot falco at gmail dot com)
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

add_executable(beast2_server_example ${PFILES})

target_compile_definitions(beast2_server_example
    PRIVATE BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET beast2_server_example
    PROPERTY FOLDER "examples")

target_include_directories(beast2_server_example PRIVATE .)
target_link_libraries(
    beast2_server_example
    Boost::beast2
    Boost::json
    Boost::url
    )

if (TARGET Boost::capy_zlib)
    target_link_libraries(beast2_server_example Boost::capy_zlib)
endif()

if (TARGET Boost::capy_brotli)
    target_link_libraries(beast2_server_example Boost::capy_brotli)
endif()
