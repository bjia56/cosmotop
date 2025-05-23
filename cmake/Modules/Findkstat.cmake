# SPDX-License-Identifier: Apache-2.0
#
# Find libkstat, the kernel statistics facility
#

if(CMAKE_SYSTEM MATCHES "SunOS")
  find_path(kstat_INCLUDE_DIR NAMES kstat.h)
  find_library(kstat_LIBRARY NAMES kstat)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(kstat REQUIRED_VARS kstat_LIBRARY kstat_INCLUDE_DIR)

  if(kstat_FOUND AND NOT TARGET kstat::kstat)
    add_library(kstat::kstat UNKNOWN IMPORTED)
    set_target_properties(kstat::kstat PROPERTIES
      IMPORTED_LOCATION "${kstat_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${kstat_INCLUDE_DIR}"
    )
  endif()

  mark_as_advanced(kstat_INCLUDE_DIR kstat_LIBRARY)
endif()

