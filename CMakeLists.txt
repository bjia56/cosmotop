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
target_compile_definitions(btop PRIVATE BTOP_PLUGIN_HOST COSMO_PLUGIN_DEBUG_RPC)

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
find_package(Threads REQUIRED)
target_link_libraries(btop Threads::Threads)

target_compile_definitions(btop PRIVATE GPU_SUPPORT FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)

# Check if lowdown is installed
find_program(LOWDOWN_EXECUTABLE lowdown)

if(LOWDOWN_EXECUTABLE)
  # Custom target to compile Markdown to man page using lowdown
  add_custom_target(btop.1 ALL
      COMMAND lowdown -s -Tman -o btop.1 manpage.md
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  # Install the man page
  install(FILES btop.1 DESTINATION "share/man/man1")
else()
  message(WARNING "Command 'lowdown' not found: skipping generating man page btop.1")
endif()

install(TARGETS btop RUNTIME)
install(FILES "btop.desktop" DESTINATION "share/applications")
install(FILES "Img/icon.png" DESTINATION "share/icons/hicolor/48x48/apps" RENAME "btop.png")
install(FILES "Img/icon.svg" DESTINATION "share/icons/hicolor/scalable/apps" RENAME "btop.svg")
install(DIRECTORY "themes" DESTINATION "share/btop")