# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for btop plugins
#

cmake_minimum_required(VERSION 3.24)

# Disable in-source builds
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

project(plugin
  DESCRIPTION "A monitor of resources"
  HOMEPAGE_URL "https://github.com/aristocratos/btop"
  LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/libcosmo_plugin)

find_package (Threads REQUIRED)

set(PLUGIN_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/btop_collect.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../btop_plugin.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_gpu_top/intel_gpu_top.c
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_gpu_top/igt_perf.c
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_gpu_top/intel_device_info.c
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_gpu_top/intel_name_lookup_shim.c
  ${LIBCOSMO_PLUGIN_SOURCES}
)

if(BUILD_EXE)
  if(NOT DEFINED BINARY_NAME)
    set(BINARY_NAME "libbtop.exe")
  endif()

  add_executable(${BINARY_NAME} ${PLUGIN_SOURCES})
else()
  if(NOT DEFINED BINARY_NAME)
    set(BINARY_NAME "btop")
  endif()

  add_library(${BINARY_NAME} SHARED ${PLUGIN_SOURCES})
endif()

target_include_directories(${BINARY_NAME} PRIVATE
  include
  ${LIBCOSMO_PLUGIN_INCLUDE_DIRS}
)
target_compile_definitions(${BINARY_NAME} PRIVATE GPU_SUPPORT FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)
if(WIN32)
  target_link_libraries(${BINARY_NAME} PUBLIC Threads::Threads wsock32 ws2_32)
else()
  target_link_libraries(${BINARY_NAME} PUBLIC Threads::Threads)
endif()

# error on undefined symbols
if(NOT WIN32)
  target_link_options(${BINARY_NAME} PRIVATE -Wl,--no-undefined)
endif()