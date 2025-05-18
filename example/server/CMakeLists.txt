#
# Copyright (c) 2023 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/CPPAlliance/http_io
#

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS *.cpp *.hpp
    CMakeLists.txt
    Jamfile)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${PFILES})

add_executable(http_io_server_example ${PFILES})

target_compile_definitions(http_io_server_example
    PRIVATE BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET http_io_server_example
    PROPERTY FOLDER "examples")

target_link_libraries(http_io_server_example
    boost_http_io)
