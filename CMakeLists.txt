#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/vinniefalco/http_io
#

cmake_minimum_required(VERSION 3.8...3.16)

set(BOOST_HTTP_IO_VERSION 1)
if(BOOST_SUPERPROJECT_VERSION)
    set(BOOST_HTTP_IO_VERSION ${BOOST_SUPERPROJECT_VERSION})
endif()

project(boost_http_io VERSION "${BOOST_HTTP_IO_VERSION}" LANGUAGES CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
    option(BOOST_HTTP_IO_BUILD_TESTS "Build boost::http_io tests" ${BUILD_TESTING})
    set(BOOST_HTTP_IO_IS_ROOT ON)
else()
    set(BOOST_HTTP_IO_BUILD_TESTS ${BUILD_TESTING})
    set(BOOST_HTTP_IO_IS_ROOT OFF)
endif()

include(GNUInstallDirs)
if(BOOST_HTTP_IO_IS_ROOT)
    set(BOOST_INCLUDE_LIBRARIES http_io http_proto program_options url)
    set(BOOST_EXCLUDE_LIBRARIES http_io)
    set(CMAKE_FOLDER Dependencies)
    add_subdirectory(../.. Dependencies/boost EXCLUDE_FROM_ALL)
    unset(CMAKE_FOLDER)
    set(BOOST_HTTP_IO_BUILD_EXAMPLES ON)
else()
    set(BOOST_HTTP_IO_BUILD_EXAMPLES OFF)
endif()

function(boost_http_io_setup_properties target)
    target_compile_features(${target} PUBLIC cxx_constexpr)
    target_compile_definitions(${target} PUBLIC BOOST_HTTP_IO_NO_LIB=1)

    if(BOOST_SUPERPROJECT_VERSION)
        target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include")
    else()
        target_include_directories(${target}
            PUBLIC
                "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )
    endif()

    target_link_libraries(${target}
        PUBLIC
            Boost::asio
            Boost::assert
            Boost::config
            Boost::http_proto
            Boost::system
            Boost::url
    )

    if(MINGW)
      target_link_libraries(${target} PUBLIC ws2_32 wsock32)
    endif()
endfunction()

file(GLOB_RECURSE BOOST_HTTP_IO_HEADERS $<$<VERSION_GREATER_EQUAL:${CMAKE_VERSION},3.12>:CONFIGURE_DEPENDS>
    include/boost/*.hpp
    include/boost/*.ipp
    include/boost/*.natvis
)

file(GLOB_RECURSE BOOST_HTTP_IO_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/boost PREFIX "" FILES ${BOOST_HTTP_IO_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "http_io" FILES ${BOOST_HTTP_IO_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES Jamfile)

add_library(boost_http_io ${BOOST_HTTP_IO_HEADERS} ${BOOST_HTTP_IO_SOURCES} build/Jamfile)
target_link_libraries(boost_http_io PUBLIC Threads::Threads)
add_library(Boost::http_io ALIAS boost_http_io)
boost_http_io_setup_properties(boost_http_io)

target_compile_definitions(boost_http_io
  PUBLIC
  BOOST_HTTP_IO_NO_LIB
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(boost_http_io PUBLIC BOOST_HTTP_IO_DYN_LINK=1)
else()
    target_compile_definitions(boost_http_io PUBLIC BOOST_HTTP_IO_STATIC_LINK=1)
endif()

if(BOOST_HTTP_IO_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(BOOST_HTTP_IO_BUILD_EXAMPLES)
    add_subdirectory (example)
endif()
