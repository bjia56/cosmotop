# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for cosmotop
#

cmake_minimum_required(VERSION 3.24)

# Disable in-source builds
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

project("cosmotop"
  DESCRIPTION "Multiplatform system monitoring tool using Cosmopolitan Libc"
  HOMEPAGE_URL "https://github.com/bjia56/cosmotop"
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

if(NOT TARGET)
  set(TARGET "cosmotop")
endif()

if(${TARGET} STREQUAL "cosmotop")

  add_executable(cosmotop
    src/cosmotop.cpp
    src/cosmotop_config.cpp
    src/cosmotop_draw.cpp
    src/cosmotop_input.cpp
    src/cosmotop_menu.cpp
    src/cosmotop_shared.cpp
    src/cosmotop_theme.cpp
    src/cosmotop_tools_host.cpp
    src/cosmotop_tools_shared.cpp
    src/cosmotop_plugin.cpp
    ${LIBCOSMO_PLUGIN_SOURCES}
  )

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

  target_include_directories(cosmotop SYSTEM PRIVATE include ${LIBCOSMO_PLUGIN_INCLUDE_DIRS})

  # Enable pthreads
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  target_link_libraries(cosmotop Threads::Threads)

  target_compile_definitions(cosmotop PRIVATE GPU_SUPPORT FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)

elseif(${TARGET} STREQUAL "plugin")

  if(CMAKE_SYSTEM MATCHES "Linux")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/linux/cosmotop_collect.cpp
      src/linux/intel_gpu_top/intel_gpu_top.c
      src/linux/intel_gpu_top/igt_perf.c
      src/linux/intel_gpu_top/intel_device_info.c
      src/linux/intel_gpu_top/intel_name_lookup_shim.c
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
  elseif(CMAKE_SYSTEM MATCHES "Darwin")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/osx/cosmotop_collect.cpp
      src/osx/sensors.cpp
      src/osx/smc.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
  endif()

  if(BUILD_EXE)
    if(NOT DEFINED BINARY_NAME)
      set(BINARY_NAME "libcosmotop.exe")
    endif()

    add_executable(${BINARY_NAME} ${PLUGIN_SOURCES})
  else()
    if(NOT DEFINED BINARY_NAME)
      set(BINARY_NAME "cosmotop")
    endif()

    add_library(${BINARY_NAME} SHARED ${PLUGIN_SOURCES})
  endif()

  target_include_directories(${BINARY_NAME} PRIVATE
    include
    ${LIBCOSMO_PLUGIN_INCLUDE_DIRS}
  )
  target_compile_definitions(${BINARY_NAME} PRIVATE GPU_SUPPORT FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)

  target_link_libraries(${BINARY_NAME} PRIVATE Threads::Threads)
  if(CMAKE_SYSTEM MATCHES "Darwin")
    target_link_libraries(${BINARY_NAME} PRIVATE $<LINK_LIBRARY:FRAMEWORK,CoreFoundation> $<LINK_LIBRARY:FRAMEWORK,IOKit>)
  endif()

endif() # TARGET