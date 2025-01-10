# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for btop
#

cmake_minimum_required(VERSION 3.24)

# Disable in-source builds since they would override the Makefile
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

project("btop"
  DESCRIPTION "A monitor of resources"
  HOMEPAGE_URL "https://github.com/aristocratos/btop"
  LANGUAGES CXX C
)

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/libcosmo_plugin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(btop
  src/btop.cpp
  src/btop_config.cpp
  src/btop_draw.cpp
  src/btop_input.cpp
  src/btop_menu.cpp
  src/btop_shared.cpp
  src/btop_theme.cpp
  src/btop_tools.cpp
  src/btop_plugin.cpp
  ${LIBCOSMO_PLUGIN_SOURCES}
)
target_compile_definitions(btop PRIVATE BTOP_PLUGIN_HOST)

# Generate build info
execute_process(
  COMMAND "git" "rev-parse" "--short" "HEAD"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set(CONFIGURE_COMMAND
    "cmake -DBTOP_STATIC=${BTOP_STATIC} -DBTOP_USE_MOLD=${BTOP_USE_MOLD} -DBTOP_FORTIFY=${BTOP_FORTIFY} -DBTOP_GPU=${BTOP_GPU}"
)
get_filename_component(CXX_COMPILER_BASENAME "${CMAKE_CXX_COMPILER}" NAME)
set(COMPILER "${CXX_COMPILER_BASENAME}")
set(COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY IMMEDIATE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

target_include_directories(btop SYSTEM PRIVATE include ${LIBCOSMO_PLUGIN_INCLUDE_DIRS})

# Enable pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(btop Threads::Threads)

target_compile_definitions(btop PRIVATE GPU_SUPPORT FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)