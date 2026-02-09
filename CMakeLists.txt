# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for cosmotop
#

cmake_minimum_required(VERSION 3.21)

# Disable in-source builds
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

project("cosmotop"
  DESCRIPTION "Multiplatform system monitoring tool using Cosmopolitan Libc"
  HOMEPAGE_URL "https://github.com/bjia56/cosmotop"
  LANGUAGES CXX C
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
include(CheckIncludeFileCXX)
include(cmake/PlatformSources.cmake)

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/libcosmo_plugin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(THIRD_PARTY_INCLUDES
  ${LIBCOSMO_PLUGIN_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/third_party/cpp-httplib
  ${PROJECT_SOURCE_DIR}/third_party/TinyMCP/Source/Protocol
  ${PROJECT_SOURCE_DIR}/third_party/TinyMCP/Source/External/jsoncpp/include
  ${PROJECT_SOURCE_DIR}/third_party/fmt/include
  ${PROJECT_SOURCE_DIR}/third_party/widecharwidth
  ${PROJECT_SOURCE_DIR}/third_party/range-v3/include
)

# Generate build info
if(NOT RELEASE)
  execute_process(
    COMMAND "git" "rev-parse" "--short" "HEAD"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
get_filename_component(CXX_COMPILER_BASENAME "${CMAKE_CXX_COMPILER}" NAME)
set(COMPILER "${CXX_COMPILER_BASENAME}")
set(COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY IMMEDIATE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT TARGET)
  set(TARGET "host")
endif()

if(${TARGET} STREQUAL "host")

  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

  add_executable(cosmotop.com
    src/cosmotop.cpp
    src/cosmotop_config.cpp
    src/cosmotop_draw.cpp
    src/cosmotop_input.cpp
    src/cosmotop_menu.cpp
    src/cosmotop_mcp.cpp
    src/cosmotop_shared.cpp
    src/cosmotop_theme.cpp
    src/cosmotop_tools_host.cpp
    src/cosmotop_tools_shared.cpp
    src/cosmotop_plugin.cpp
    ${LIBCOSMO_PLUGIN_SOURCES}
    third_party/TinyMCP/Source/Protocol/Entity/Server.cpp
    third_party/TinyMCP/Source/Protocol/Message/BasicMessage.cpp
    third_party/TinyMCP/Source/Protocol/Message/Message.cpp
    third_party/TinyMCP/Source/Protocol/Message/Notification.cpp
    third_party/TinyMCP/Source/Protocol/Message/Request.cpp
    third_party/TinyMCP/Source/Protocol/Message/Response.cpp
    third_party/TinyMCP/Source/Protocol/Session/Session.cpp
    third_party/TinyMCP/Source/Protocol/Task/BasicTask.cpp
    third_party/TinyMCP/Source/Protocol/Transport/Transport.cpp
    third_party/TinyMCP/Source/External/jsoncpp/src/lib_json/json_reader.cpp
    third_party/TinyMCP/Source/External/jsoncpp/src/lib_json/json_value.cpp
    third_party/TinyMCP/Source/External/jsoncpp/src/lib_json/json_writer.cpp
  )

  target_compile_options(cosmotop.com PRIVATE -mcosmo)
  target_include_directories(cosmotop.com SYSTEM PRIVATE ${THIRD_PARTY_INCLUDES})

  # Enable pthreads
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)

  set(LINK_LIBS
    Threads::Threads
  )
  set(COMPILE_DEFINITIONS
    FMT_HEADER_ONLY
    _FILE_OFFSET_BITS=64
  )

  target_link_libraries(cosmotop.com PRIVATE ${LINK_LIBS})
  target_compile_definitions(cosmotop.com PRIVATE ${COMPILE_DEFINITIONS})

elseif(${TARGET} STREQUAL "plugin")

  set(BUILD_EXE ON)

  # Windows doesn't exit properly
  if(CMAKE_SYSTEM MATCHES "Windows")
    set(BUILD_EXE OFF)
  endif()

  set(PLUGIN_SOURCES
    src/cosmotop_plugin.cpp
    src/cosmotop_shared.cpp
    src/cosmotop_tools_shared.cpp
    ${LIBCOSMO_PLUGIN_SOURCES}
  )

  if(BUILD_EXE)
    if(CMAKE_SYSTEM MATCHES "Windows")
      set(BINARY_NAME "cosmotop-plugin")
    else()
      set(BINARY_NAME "cosmotop-plugin.exe")
    endif()
    add_executable(${BINARY_NAME} ${PLUGIN_SOURCES})
    target_compile_definitions(${BINARY_NAME} PRIVATE COSMO_PLUGIN_WANT_MAIN)
    if(CMAKE_SYSTEM MATCHES "Linux")
      target_compile_options(${BINARY_NAME} PRIVATE -static)
    endif()
  else()
    set(BINARY_NAME "cosmotop-plugin")
    add_library(${BINARY_NAME} SHARED ${PLUGIN_SOURCES})
  endif()

  # Add platform-specific sources and configuration
  cosmotop_add_platform_sources(${BINARY_NAME})

  target_include_directories(${BINARY_NAME} PRIVATE ${THIRD_PARTY_INCLUDES})
  target_compile_definitions(${BINARY_NAME} PRIVATE FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)
  if(CMAKE_SYSTEM MATCHES "Windows")
    target_compile_options(${BINARY_NAME} PRIVATE /utf-8)
    target_compile_definitions(${BINARY_NAME} PRIVATE UNICODE _UNICODE)
  elseif(CMAKE_SYSTEM MATCHES "Haiku")
    target_compile_definitions(${BINARY_NAME} PRIVATE _DEFAULT_SOURCE)
  endif()

  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)

  target_link_libraries(${BINARY_NAME} PRIVATE Threads::Threads)

  # Add platform-specific libraries
  cosmotop_link_platform_libraries(${BINARY_NAME})

elseif(${TARGET} STREQUAL "standalone")

  set(REFLECTCPP_SOURCES
    third_party/reflect-cpp/src/reflectcpp.cpp
    third_party/reflect-cpp/src/reflectcpp_json.cpp
    third_party/reflect-cpp/src/yyjson.c
  )

  add_executable(cosmotop
    src/cosmotop.cpp
    src/cosmotop_config.cpp
    src/cosmotop_draw.cpp
    src/cosmotop_input.cpp
    src/cosmotop_menu.cpp
    src/cosmotop_mcp.cpp
    src/cosmotop_shared.cpp
    src/cosmotop_theme.cpp
    src/cosmotop_tools_host.cpp
    src/cosmotop_tools_shared.cpp
    src/cosmotop_plugin.cpp
    ${REFLECTCPP_SOURCES}
    third_party/TinyMCP/Source/Protocol/Entity/Server.cpp
    third_party/TinyMCP/Source/Protocol/Message/BasicMessage.cpp
    third_party/TinyMCP/Source/Protocol/Message/Message.cpp
    third_party/TinyMCP/Source/Protocol/Message/Notification.cpp
    third_party/TinyMCP/Source/Protocol/Message/Request.cpp
    third_party/TinyMCP/Source/Protocol/Message/Response.cpp
    third_party/TinyMCP/Source/Protocol/Session/Session.cpp
    third_party/TinyMCP/Source/Protocol/Task/BasicTask.cpp
    third_party/TinyMCP/Source/Protocol/Transport/Transport.cpp
    third_party/TinyMCP/Source/External/jsoncpp/src/lib_json/json_reader.cpp
    third_party/TinyMCP/Source/External/jsoncpp/src/lib_json/json_value.cpp
    third_party/TinyMCP/Source/External/jsoncpp/src/lib_json/json_writer.cpp
  )

  target_include_directories(cosmotop PRIVATE ${THIRD_PARTY_INCLUDES})
  target_include_directories(cosmotop PRIVATE third_party/reflect-cpp/include)

  # Add platform-specific sources and configuration
  cosmotop_add_platform_sources(cosmotop)

  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)

  target_link_libraries(cosmotop PRIVATE Threads::Threads)

  target_compile_definitions(cosmotop PRIVATE
    COSMOTOP_STANDALONE
    FMT_HEADER_ONLY
    _FILE_OFFSET_BITS=64
  )

  # Add platform-specific libraries
  cosmotop_link_platform_libraries(cosmotop)

endif() # TARGET
