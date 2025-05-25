#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 Dmitry Arkhipov (grisumbras@gmail.com)
# Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2025 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/http_io
#

#-------------------------------------------------
#
# Project
#
#-------------------------------------------------
cmake_minimum_required(VERSION 3.8...3.20)
set(BOOST_HTTP_IO_VERSION 1)
if (BOOST_SUPERPROJECT_VERSION)
    set(BOOST_HTTP_IO_VERSION ${BOOST_SUPERPROJECT_VERSION})
endif ()
project(boost_http_io VERSION "${BOOST_HTTP_IO_VERSION}" LANGUAGES CXX)
set(BOOST_HTTP_IO_IS_ROOT OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BOOST_HTTP_IO_IS_ROOT ON)
endif ()
set(__ignore__ ${CMAKE_C_COMPILER})

#-------------------------------------------------
#
# Options
#
#-------------------------------------------------
if (BOOST_HTTP_IO_IS_ROOT)
    include(CTest)
endif ()
option(BOOST_HTTP_IO_BUILD_TESTS "Build boost::http_io tests" ${BUILD_TESTING})
option(BOOST_HTTP_IO_BUILD_EXAMPLES "Build boost::http_io examples" ${BOOST_HTTP_IO_IS_ROOT})


# Check if environment variable BOOST_SRC_DIR is set
if (NOT DEFINED BOOST_SRC_DIR AND DEFINED ENV{BOOST_SRC_DIR})
    set(DEFAULT_BOOST_SRC_DIR "$ENV{BOOST_SRC_DIR}")
else ()
    set(DEFAULT_BOOST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
endif ()
set(BOOST_SRC_DIR ${DEFAULT_BOOST_SRC_DIR} CACHE STRING "Boost source dir to use when running CMake from this directory")

#-------------------------------------------------
#
# Boost modules
#
#-------------------------------------------------
# The boost super-project requires one explicit dependency per-line.
set(BOOST_HTTP_IO_DEPENDENCIES
    Boost::asio
    Boost::assert
    Boost::config
    Boost::http_proto
    Boost::system
    Boost::throw_exception
    )

foreach (BOOST_HTTP_IO_DEPENDENCY ${BOOST_HTTP_IO_DEPENDENCIES})
    if (BOOST_HTTP_IO_DEPENDENCY MATCHES "^[ ]*Boost::([A-Za-z0-9_]+)[ ]*$")
        list(APPEND BOOST_HTTP_IO_INCLUDE_LIBRARIES ${CMAKE_MATCH_1})
    endif ()
endforeach ()
# Conditional dependencies
if (BOOST_HTTP_IO_BUILD_TESTS)
    set(BOOST_HTTP_IO_UNIT_TEST_LIBRARIES beast url)
endif ()
if (BOOST_HTTP_IO_BUILD_EXAMPLES)
    set(BOOST_HTTP_IO_EXAMPLE_LIBRARIES program_options scope url)
endif ()
# Complete dependency list
set(BOOST_INCLUDE_LIBRARIES ${BOOST_HTTP_IO_INCLUDE_LIBRARIES} ${BOOST_HTTP_IO_UNIT_TEST_LIBRARIES} ${BOOST_HTTP_IO_EXAMPLE_LIBRARIES})
set(BOOST_EXCLUDE_LIBRARIES http_io)

#-------------------------------------------------
#
# Add Boost Subdirectory
#
#-------------------------------------------------
if (BOOST_HTTP_IO_IS_ROOT)
    set(CMAKE_FOLDER Dependencies)
    # Find absolute BOOST_SRC_DIR
    if (NOT IS_ABSOLUTE ${BOOST_SRC_DIR})
        set(BOOST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${BOOST_SRC_DIR}")
    endif ()

    # Validate BOOST_SRC_DIR
    set(BOOST_SRC_DIR_IS_VALID ON)
    foreach (F "CMakeLists.txt" "Jamroot" "boost-build.jam" "bootstrap.sh" "libs")
        if (NOT EXISTS "${BOOST_SRC_DIR}/${F}")
            message(STATUS "${BOOST_SRC_DIR}/${F} does not exist. Fallback to find_package.")
            set(BOOST_SRC_DIR_IS_VALID OFF)
            break()
        endif ()
    endforeach ()

    # Create Boost interface targets
    if (BOOST_SRC_DIR_IS_VALID)
        # From BOOST_SRC_DIR
        if (BUILD_SHARED_LIBS)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
        endif ()
        set(BOOST_EXCLUDE_LIBRARIES ${PROJECT_NAME})
        set(PREV_BUILD_TESTING ${BUILD_TESTING})
        set(BUILD_TESTING OFF CACHE BOOL "Build the tests." FORCE)
        add_subdirectory(${BOOST_SRC_DIR} Dependencies/boost EXCLUDE_FROM_ALL)
        set(BUILD_TESTING ${PREV_BUILD_TESTING} CACHE BOOL "Build the tests." FORCE)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${BOOST_SRC_DIR}/tools/cmake/include")
    else ()
        # From Boost Package
        find_package(Boost REQUIRED COMPONENTS buffers http_proto program_options scope url)
        foreach (BOOST_INCLUDE_LIBRARY ${BOOST_INCLUDE_LIBRARIES})
            if (NOT TARGET Boost::${BOOST_INCLUDE_LIBRARY})
                add_library(Boost::${BOOST_INCLUDE_LIBRARY} ALIAS Boost::headers)
            endif ()
        endforeach ()
    endif ()
    unset(CMAKE_FOLDER)
endif ()

#-------------------------------------------------
#
# Library
#
#-------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB_RECURSE BOOST_HTTP_IO_HEADERS CONFIGURE_DEPENDS include/boost/http_io/*.hpp include/boost/*.natvis)
file(GLOB_RECURSE BOOST_HTTP_IO_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

source_group("" FILES "include/boost/http_io.hpp" "build/Jamfile")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/boost/http_io PREFIX "include" FILES ${BOOST_HTTP_IO_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" FILES ${BOOST_HTTP_IO_SOURCES})

add_library(boost_http_io include/boost/http_io.hpp build/Jamfile ${BOOST_HTTP_IO_HEADERS} ${BOOST_HTTP_IO_SOURCES})
add_library(Boost::http_io ALIAS boost_http_io)
target_compile_features(boost_http_io PUBLIC cxx_constexpr)
target_include_directories(boost_http_io PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(boost_http_io PUBLIC ${BOOST_HTTP_IO_DEPENDENCIES})
find_package(Threads REQUIRED)
target_link_libraries(boost_http_io PUBLIC Threads::Threads)
if (MINGW)
    target_link_libraries(boost_http_io PUBLIC ws2_32 wsock32)
endif()
target_compile_definitions(boost_http_io PUBLIC BOOST_HTTP_IO_NO_LIB)
target_compile_definitions(boost_http_io PRIVATE BOOST_HTTP_IO_SOURCE)
if (BUILD_SHARED_LIBS)
    target_compile_definitions(boost_http_io PUBLIC BOOST_HTTP_IO_DYN_LINK)
else ()
    target_compile_definitions(boost_http_io PUBLIC BOOST_HTTP_IO_STATIC_LINK)
endif ()

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------
if (BOOST_HTTP_IO_BUILD_TESTS)
    add_subdirectory(test)
endif ()

#-------------------------------------------------
#
# Examples
#
#-------------------------------------------------
if (BOOST_HTTP_IO_BUILD_EXAMPLES)
    add_subdirectory(example)
endif ()
