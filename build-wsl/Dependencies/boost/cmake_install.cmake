# Install script for directory: /home/mungo/winrepos/MungoG/cursor_boost

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/algorithm/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/align/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/any/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/array/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/asio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/assert/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/atomic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/bind/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/capy/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/concept_check/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/config/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/container/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/container_hash/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/context/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/conversion/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/core/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/corosio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/date_time/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/describe/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/detail/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/dynamic_bitset/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/endian/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/exception/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/filesystem/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/function/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/function_types/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/functional/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/fusion/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/headers/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/http/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/integer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/intrusive/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/io/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/iterator/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/json/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/lexical_cast/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/math/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/move/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/mp11/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/mpl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/multiprecision/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/numeric/conversion/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/optional/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/pool/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/predef/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/preprocessor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/program_options/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/random/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/range/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/regex/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/scope/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/smart_ptr/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/static_assert/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/system/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/throw_exception/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/tokenizer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/tuple/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/type_index/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/type_traits/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/typeof/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/unordered/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/url/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/utility/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/variant2/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/mungo/winrepos/MungoG/cursor_boost/libs/beast2/build-wsl/Dependencies/boost/libs/winapi/cmake_install.cmake")
endif()

