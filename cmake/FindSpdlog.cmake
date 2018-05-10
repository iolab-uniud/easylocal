# Try to find the spdlog libraries.
#
# Once done this will define
# SPDLOG_FOUND - System has EasyLocal
# SPDLOG_INCLUDE_DIRS - The include directories

message(STATUS "Searching for spdlog")

if (SPDLOG_FOUND)
  return()
endif()

find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h)
set(SPDLOG_INCLUDE_DIRS ${SPDLOG_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set SPDLOG_FOUND to TRUE
# if all listed variables are TRUE.
find_package_handle_standard_args(spdlog DEFAULT_MSG SPDLOG_INCLUDE_DIRS)
