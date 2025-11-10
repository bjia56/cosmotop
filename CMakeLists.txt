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
  ${PROJECT_SOURCE_DIR}/third_party/date/include
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
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    if(RSMI_STATIC)
      # ROCm doesn't properly add it's folders to the module path if `CMAKE_MODULE_PATH` is already
      # set
      # We could also manually append ROCm's path here
      set(_CMAKE_MODULE_PATH CMAKE_MODULE_PATH)
      unset(CMAKE_MODULE_PATH)

      # Build a static ROCm library
      set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

      add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/rocm_smi_lib EXCLUDE_FROM_ALL)

      add_library(ROCm INTERFACE)
      # Export ROCm's properties to a target
      target_compile_definitions(ROCm INTERFACE RSMI_STATIC)
      target_include_directories(ROCm INTERFACE ${PROJECT_SOURCE_DIR}/third_party/rocm_smi_lib/include)
      target_link_libraries(ROCm INTERFACE rocm_smi64)

      set(CMAKE_MODULE_PATH _CMAKE_MODULE_PATH)
    endif()
  elseif(CMAKE_SYSTEM MATCHES "Darwin")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/osx/cosmotop_collect.cpp
      src/osx/sensors.cpp
      src/osx/smc.cpp
      src/osx/ioreport.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
      set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")
    else()
      set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
    endif()
  elseif(CMAKE_SYSTEM MATCHES "Windows")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/windows/cosmotop_collect.cpp
      src/windows/docker_http_client.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )

    # Windows doesn't exit properly
    set(BUILD_EXE OFF)

    # range/v3 doesn't compile properly with MSVC, so prefer stdlib ranges on Windows
    check_include_file_cxx(ranges CXX_HAVE_RANGES)
    if(NOT CXX_HAVE_RANGES)
      message(FATAL_ERROR "The compiler doesn't support <ranges>")
    endif()
  elseif(CMAKE_SYSTEM MATCHES "FreeBSD" OR CMAKE_SYSTEM MATCHES "MidnightBSD")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/freebsd/cosmotop_collect.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
    find_package(devstat REQUIRED)
    find_package(kvm REQUIRED)
    find_package(elf REQUIRED)
  elseif(CMAKE_SYSTEM MATCHES "DragonFly")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/dragonfly/cosmotop_collect.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
    find_package(devstat REQUIRED)
    find_package(kvm REQUIRED)
  elseif(CMAKE_SYSTEM MATCHES "NetBSD")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/netbsd/cosmotop_collect.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
    find_package(kvm REQUIRED)
    find_package(proplib REQUIRED)
  elseif(CMAKE_SYSTEM MATCHES "OpenBSD")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/openbsd/cosmotop_collect.cpp
      src/openbsd/sysctlbyname.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
    find_package(kvm REQUIRED)
  elseif(CMAKE_SYSTEM MATCHES "SunOS")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/solaris/cosmotop_collect.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
    find_package(kstat REQUIRED)
  elseif(CMAKE_SYSTEM MATCHES "Haiku")
    set(PLUGIN_SOURCES
      src/cosmotop_plugin.cpp
      src/cosmotop_shared.cpp
      src/cosmotop_tools_shared.cpp
      src/haiku/cosmotop_collect.cpp
      ${LIBCOSMO_PLUGIN_SOURCES}
    )
  else()
    message(FATAL_ERROR "Unsupported platform")
  endif()

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
  if((NOT CMAKE_SYSTEM MATCHES "Darwin") AND (NOT CMAKE_SYSTEM MATCHES "Windows") AND (NOT CMAKE_SYSTEM MATCHES "SunOS") AND (NOT CMAKE_SYSTEM MATCHES "Haiku"))
    target_link_libraries(${BINARY_NAME} PRIVATE -static-libgcc -static-libstdc++)
  endif()

  if(CMAKE_SYSTEM MATCHES "Linux" AND RSMI_STATIC)
    target_link_libraries(${BINARY_NAME} PRIVATE ROCm)
  elseif(CMAKE_SYSTEM MATCHES "Darwin")
    set(PLUGIN_LIBRARIES
      $<LINK_LIBRARY:FRAMEWORK,CoreFoundation>
      $<LINK_LIBRARY:FRAMEWORK,IOKit>
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
      message(STATUS "Target is arm64/aarch64, adding IOReport")
      list(APPEND PLUGIN_LIBRARIES IOReport)
    endif()
    target_link_libraries(${BINARY_NAME} PRIVATE ${PLUGIN_LIBRARIES})
  elseif(CMAKE_SYSTEM MATCHES "FreeBSD" OR CMAKE_SYSTEM MATCHES "MidnightBSD")
    target_link_libraries(${BINARY_NAME} PRIVATE devstat::devstat kvm::kvm elf::elf)
  elseif(CMAKE_SYSTEM MATCHES "DragonFly")
    target_link_libraries(${BINARY_NAME} PRIVATE devstat::devstat kvm::kvm)
  elseif(CMAKE_SYSTEM MATCHES "NetBSD")
    target_link_libraries(${BINARY_NAME} PRIVATE kvm::kvm proplib::proplib)
  elseif(CMAKE_SYSTEM MATCHES "OpenBSD")
    target_link_libraries(${BINARY_NAME} PRIVATE kvm::kvm)
  elseif(CMAKE_SYSTEM MATCHES "SunOS")
    target_link_libraries(${BINARY_NAME} PRIVATE kstat::kstat)
  elseif(CMAKE_SYSTEM MATCHES "Haiku")
    target_link_libraries(${BINARY_NAME} PRIVATE be root bnetapi network)
  endif()

endif() # TARGET
