#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 Dmitry Arkhipov (grisumbras@gmail.com)
# Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2025 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/beast2
#

#-------------------------------------------------
#
# Project
#
#-------------------------------------------------
cmake_minimum_required(VERSION 3.8...3.20)
set(BOOST_BEAST2_VERSION 1)
if (BOOST_SUPERPROJECT_VERSION)
    set(BOOST_BEAST2_VERSION ${BOOST_SUPERPROJECT_VERSION})
endif ()
project(boost_beast2 VERSION "${BOOST_BEAST2_VERSION}" LANGUAGES CXX)
set(BOOST_BEAST2_IS_ROOT OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BOOST_BEAST2_IS_ROOT ON)
endif ()
set(__ignore__ ${CMAKE_C_COMPILER})

#-------------------------------------------------
#
# Options
#
#-------------------------------------------------
if (BOOST_BEAST2_IS_ROOT)
    include(CTest)
endif ()
option(BOOST_BEAST2_BUILD_TESTS "Build boost::beast2 tests" ${BUILD_TESTING})
option(BOOST_BEAST2_BUILD_EXAMPLES "Build boost::beast2 examples" ${BOOST_BEAST2_IS_ROOT})
option(BOOST_BEAST2_MRDOCS_BUILD "Build the target for MrDocs: see mrdocs.yml" OFF)


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
set(BOOST_BEAST2_DEPENDENCIES
    Boost::asio
    Boost::assert
    Boost::capy
    Boost::config
    Boost::core
    Boost::describe
    Boost::endian
    Boost::http
    Boost::json
    Boost::mp11
    Boost::static_assert
    Boost::system
    Boost::throw_exception
    Boost::url
    Boost::variant2)

foreach (BOOST_BEAST2_DEPENDENCY ${BOOST_BEAST2_DEPENDENCIES})
    if (BOOST_BEAST2_DEPENDENCY MATCHES "^[ ]*Boost::([A-Za-z0-9_]+)[ ]*$")
        list(APPEND BOOST_BEAST2_INCLUDE_LIBRARIES ${CMAKE_MATCH_1})
    endif ()
endforeach ()
# Conditional dependencies
if (BOOST_BEAST2_BUILD_TESTS)
    set(BOOST_BEAST2_UNIT_TEST_LIBRARIES beast)
endif ()
if (BOOST_BEAST2_BUILD_EXAMPLES)
    set(BOOST_BEAST2_EXAMPLE_LIBRARIES program_options scope multiprecision)
endif ()
# Complete dependency list
set(BOOST_INCLUDE_LIBRARIES ${BOOST_BEAST2_INCLUDE_LIBRARIES} ${BOOST_BEAST2_UNIT_TEST_LIBRARIES} ${BOOST_BEAST2_EXAMPLE_LIBRARIES})
set(BOOST_EXCLUDE_LIBRARIES beast2)

#-------------------------------------------------
#
# Add Boost Subdirectory
#
#-------------------------------------------------
if (BOOST_BEAST2_IS_ROOT)
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
        find_package(Boost REQUIRED COMPONENTS capy http json program_options scope system url)
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

file(GLOB_RECURSE BOOST_BEAST2_HEADERS CONFIGURE_DEPENDS include/boost/beast2/*.hpp include/boost/*.natvis)
file(GLOB_RECURSE BOOST_BEAST2_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

source_group("" FILES "include/boost/beast2.hpp" "build/Jamfile")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/boost/beast2 PREFIX "include" FILES ${BOOST_BEAST2_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" FILES ${BOOST_BEAST2_SOURCES})

function(boost_beast2_setup_properties target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_include_directories(${target}
        PUBLIC "${PROJECT_SOURCE_DIR}/include"
        PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(${target} PUBLIC ${BOOST_BEAST2_DEPENDENCIES})
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PUBLIC Threads::Threads)
    if (MINGW)
        target_link_libraries(${target} PUBLIC ws2_32 wsock32)
    endif()
    target_compile_definitions(${target} PUBLIC BOOST_BEAST2_NO_LIB)
    target_compile_definitions(${target} PRIVATE BOOST_BEAST2_SOURCE)
    if (BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PUBLIC BOOST_BEAST2_DYN_LINK)
    else ()
        target_compile_definitions(${target} PUBLIC BOOST_BEAST2_STATIC_LINK)
    endif ()
endfunction()

if (BOOST_BEAST2_MRDOCS_BUILD)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mrdocs.cpp" "#include <boost/beast2.hpp>\n")
    add_library(boost_beast2_mrdocs "${CMAKE_CURRENT_BINARY_DIR}/mrdocs.cpp")
    boost_beast2_setup_properties(boost_beast2_mrdocs)
    target_compile_definitions(boost_beast2_mrdocs PUBLIC BOOST_BEAST2_MRDOCS)
    set_target_properties(boost_beast2_mrdocs PROPERTIES EXPORT_COMPILE_COMMANDS ON)
    return()
endif ()

add_library(boost_beast2 include/boost/beast2.hpp build/Jamfile ${BOOST_BEAST2_HEADERS} ${BOOST_BEAST2_SOURCES})
add_library(Boost::beast2 ALIAS boost_beast2)
boost_beast2_setup_properties(boost_beast2)

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------
if (BOOST_BEAST2_BUILD_TESTS)
    add_subdirectory(test)
endif ()

#-------------------------------------------------
#
# Examples
#
#-------------------------------------------------
if (BOOST_BEAST2_BUILD_EXAMPLES)
    add_subdirectory(example)
endif ()
