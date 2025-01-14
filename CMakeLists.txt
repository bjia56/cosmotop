# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for btop
#

cmake_minimum_required(VERSION 3.24)

# Disable in-source builds
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

find_package(Threads REQUIRED)

if(NOT BTOP_TARGET)
  set(BTOP_TARGET "btop")
endif()

if(BTOP_TARGET STREQUAL "btop")

  add_executable(btop
    src/btop.cpp
    src/btop_config.cpp
    src/btop_draw.cpp
    src/btop_input.cpp
    src/btop_menu.cpp
    src/btop_shared.cpp
    src/btop_theme.cpp
    src/btop_tools_host.cpp
    src/btop_tools_shared.cpp
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
  get_filename_component(CXX_COMPILER_BASENAME "${CMAKE_CXX_COMPILER}" NAME)
  set(COMPILER "${CXX_COMPILER_BASENAME}")
  set(COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY IMMEDIATE)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)

  target_include_directories(btop SYSTEM PRIVATE include ${LIBCOSMO_PLUGIN_INCLUDE_DIRS})

  # Enable pthreads
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  target_link_libraries(btop Threads::Threads)

  target_compile_definitions(btop PRIVATE GPU_SUPPORT FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)

elseif(BTOP_TARGET STREQUAL "plugin")

  if(CMAKE_SYSTEM MATCHES "Linux")
    set(PLUGIN_SOURCES
      src/btop_plugin.cpp
      src/btop_shared.cpp
      src/btop_tools_shared.cpp
      src/linux/btop_collect.cpp
      src/linux/intel_gpu_top/intel_gpu_top.c
      src/linux/intel_gpu_top/igt_perf.c
      src/linux/intel_gpu_top/intel_device_info.c
      src/linux/intel_gpu_top/intel_name_lookup_shim.c
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
  endif()

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
  target_compile_definitions(${BINARY_NAME} PRIVATE
    BTOP_PLUGIN
    GPU_SUPPORT
    FMT_HEADER_ONLY
    _FILE_OFFSET_BITS=64
  )

  target_link_libraries(${BINARY_NAME} PUBLIC Threads::Threads)

  # error on undefined symbols
  if(NOT WIN32)
    target_link_options(${BINARY_NAME} PRIVATE -Wl,--no-undefined)
  endif()

endif() # BTOP_TARGET