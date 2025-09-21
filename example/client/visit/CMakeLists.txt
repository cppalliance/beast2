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
    PRIVATE
    BOOST_ASIO_NO_DEPRECATED)

set_property(TARGET http_io_example_client_visit
    PROPERTY FOLDER "examples")

target_link_libraries(http_io_example_client_visit
    Boost::http_io
    Boost::url)
