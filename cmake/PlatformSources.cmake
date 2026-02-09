# PlatformSources.cmake
# Shared CMake module for platform detection and configuration
# Provides functions to add platform-specific sources and link libraries

# Platform source management for cosmotop plugin targets

# Adds platform-specific collector source files to the target
# and handles related platform configuration
function(cosmotop_add_platform_sources TARGET_NAME)
  # Determine platform-specific sources
  if(CMAKE_SYSTEM MATCHES "Linux")
    target_sources(${TARGET_NAME} PRIVATE
      src/linux/cosmotop_collect.cpp
      src/linux/intel_gpu_top/intel_gpu_top.c
      src/linux/intel_gpu_top/igt_perf.c
      src/linux/intel_gpu_top/intel_device_info.c
      src/linux/intel_gpu_top/intel_name_lookup_shim.c
    )

    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" PARENT_SCOPE)

    if(RSMI_STATIC)
      # ROCm doesn't properly add its folders to the module path if CMAKE_MODULE_PATH is already set
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

      set(CMAKE_MODULE_PATH ${_CMAKE_MODULE_PATH} PARENT_SCOPE)
    endif()

  elseif(CMAKE_SYSTEM MATCHES "Darwin")
    target_sources(${TARGET_NAME} PRIVATE
      src/osx/cosmotop_collect.cpp
      src/osx/sensors.cpp
      src/osx/smc.cpp
      src/osx/ioreport.cpp
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
      set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")
    else()
      set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
    endif()

  elseif(CMAKE_SYSTEM MATCHES "Windows")
    target_sources(${TARGET_NAME} PRIVATE
      src/windows/cosmotop_collect.cpp
      src/windows/docker_http_client.cpp
    )

    # range/v3 doesn't compile properly with MSVC, so prefer stdlib ranges on Windows
    check_include_file_cxx(ranges CXX_HAVE_RANGES)
    if(NOT CXX_HAVE_RANGES)
      message(FATAL_ERROR "The compiler doesn't support <ranges>")
    endif()

  elseif(CMAKE_SYSTEM MATCHES "FreeBSD" OR CMAKE_SYSTEM MATCHES "MidnightBSD")
    target_sources(${TARGET_NAME} PRIVATE
      src/freebsd/cosmotop_collect.cpp
    )
    find_package(devstat REQUIRED)
    find_package(kvm REQUIRED)
    find_package(elf REQUIRED)

  elseif(CMAKE_SYSTEM MATCHES "DragonFly")
    target_sources(${TARGET_NAME} PRIVATE
      src/dragonfly/cosmotop_collect.cpp
    )
    find_package(devstat REQUIRED)
    find_package(kvm REQUIRED)

  elseif(CMAKE_SYSTEM MATCHES "NetBSD")
    target_sources(${TARGET_NAME} PRIVATE
      src/netbsd/cosmotop_collect.cpp
    )
    find_package(kvm REQUIRED)
    find_package(proplib REQUIRED)

  elseif(CMAKE_SYSTEM MATCHES "OpenBSD")
    target_sources(${TARGET_NAME} PRIVATE
      src/openbsd/cosmotop_collect.cpp
      src/openbsd/sysctlbyname.cpp
    )
    find_package(kvm REQUIRED)

  elseif(CMAKE_SYSTEM MATCHES "SunOS")
    target_sources(${TARGET_NAME} PRIVATE
      src/solaris/cosmotop_collect.cpp
    )
    find_package(kstat REQUIRED)

  elseif(CMAKE_SYSTEM MATCHES "Haiku")
    target_sources(${TARGET_NAME} PRIVATE
      src/haiku/cosmotop_collect.cpp
    )

  else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM}")
  endif()
endfunction()

# Links platform-specific libraries to the target
function(cosmotop_link_platform_libraries TARGET_NAME)
  if(CMAKE_SYSTEM MATCHES "Linux")
    if(RSMI_STATIC)
      target_link_libraries(${TARGET_NAME} PRIVATE ROCm)
    endif()

  elseif(CMAKE_SYSTEM MATCHES "Darwin")
    set(PLATFORM_LIBRARIES
      $<LINK_LIBRARY:FRAMEWORK,CoreFoundation>
      $<LINK_LIBRARY:FRAMEWORK,IOKit>
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
      message(STATUS "Target is arm64/aarch64, adding IOReport")
      list(APPEND PLATFORM_LIBRARIES IOReport)
    endif()
    target_link_libraries(${TARGET_NAME} PRIVATE ${PLATFORM_LIBRARIES})

  elseif(CMAKE_SYSTEM MATCHES "FreeBSD" OR CMAKE_SYSTEM MATCHES "MidnightBSD")
    target_link_libraries(${TARGET_NAME} PRIVATE devstat::devstat kvm::kvm elf::elf)

  elseif(CMAKE_SYSTEM MATCHES "DragonFly")
    target_link_libraries(${TARGET_NAME} PRIVATE devstat::devstat kvm::kvm)

  elseif(CMAKE_SYSTEM MATCHES "NetBSD")
    target_link_libraries(${TARGET_NAME} PRIVATE kvm::kvm proplib::proplib)

  elseif(CMAKE_SYSTEM MATCHES "OpenBSD")
    target_link_libraries(${TARGET_NAME} PRIVATE kvm::kvm)

  elseif(CMAKE_SYSTEM MATCHES "SunOS")
    target_link_libraries(${TARGET_NAME} PRIVATE kstat::kstat)

  elseif(CMAKE_SYSTEM MATCHES "Haiku")
    target_link_libraries(${TARGET_NAME} PRIVATE be root bnetapi network)

  endif()

  # Static linking for non-special platforms
  if((NOT CMAKE_SYSTEM MATCHES "Darwin") AND 
     (NOT CMAKE_SYSTEM MATCHES "Windows") AND 
     (NOT CMAKE_SYSTEM MATCHES "SunOS") AND 
     (NOT CMAKE_SYSTEM MATCHES "Haiku"))
    target_link_libraries(${TARGET_NAME} PRIVATE -static-libgcc -static-libstdc++)
  endif()
endfunction()
