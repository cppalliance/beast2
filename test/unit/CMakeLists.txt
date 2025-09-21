#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/http_io
#

set(EXTRAFILES
    ../../../url/extra/test_suite/test_main.cpp
    ../../../url/extra/test_suite/test_suite.cpp
    ../../../url/extra/test_suite/test_suite.hpp
    )

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS *.cpp *.hpp)
list(APPEND PFILES
    CMakeLists.txt
    Jamfile
    )

find_package(OpenSSL REQUIRED)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${PFILES})
source_group("_extra" FILES ${EXTRAFILES})
add_executable(boost_http_io_tests ${PFILES} ${EXTRAFILES})
target_include_directories(boost_http_io_tests PRIVATE . ../../../url/extra/test_suite)
target_link_libraries(boost_http_io_tests PRIVATE
    OpenSSL::SSL
    Boost::beast
    Boost::http_io
    )

add_test(NAME boost_http_io_tests COMMAND boost_http_io_tests)
add_dependencies(tests boost_http_io_tests)
