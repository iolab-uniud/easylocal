cmake_minimum_required(VERSION 3.25)

# Attributes whose standard spelling a toolchain ignores are written through
# the macros of easylocal/utils/detail/attributes.hpp: the Microsoft ABI (MSVC,
# clang-cl) ignores [[no_unique_address]], so the raw attribute compiles there
# but leaves the member's storage in place. EASYLOCAL_SCAN_DIRS is the
# ;-separated list of directories to check (default: include, examples and
# tests of EASYLOCAL_SOURCE_DIR).

if(NOT DEFINED EASYLOCAL_SCAN_DIRS OR EASYLOCAL_SCAN_DIRS STREQUAL "")
    if(NOT DEFINED EASYLOCAL_SOURCE_DIR OR EASYLOCAL_SOURCE_DIR STREQUAL "")
        message(FATAL_ERROR "EASYLOCAL_SOURCE_DIR or EASYLOCAL_SCAN_DIRS must be provided")
    endif()
    set(EASYLOCAL_SCAN_DIRS
        "${EASYLOCAL_SOURCE_DIR}/include"
        "${EASYLOCAL_SOURCE_DIR}/examples"
        "${EASYLOCAL_SOURCE_DIR}/tests")
endif()

set(_sources)
foreach(_directory IN LISTS EASYLOCAL_SCAN_DIRS)
    file(GLOB_RECURSE _found "${_directory}/*.hpp" "${_directory}/*.cpp")
    list(APPEND _sources ${_found})
endforeach()

set(_violations)
foreach(_source IN LISTS _sources)
    # The macros' own definitions.
    if(_source MATCHES "/easylocal/utils/detail/attributes\\.hpp$")
        continue()
    endif()
    file(STRINGS "${_source}" _lines)
    set(_number 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _number "${_number} + 1")
        if(_line MATCHES "\\[\\[[ \t]*no_unique_address[ \t]*\\]\\]")
            list(APPEND _violations "${_source}:${_number}")
        endif()
    endforeach()
endforeach()

if(_violations)
    list(JOIN _violations "\n  " _report)
    message(FATAL_ERROR
        "[[no_unique_address]] is ignored by MSVC and clang-cl; write "
        "EASYLOCAL_NO_UNIQUE_ADDRESS (easylocal/utils/detail/attributes.hpp):\n  "
        "${_report}")
endif()
message(STATUS "No raw [[no_unique_address]] in ${EASYLOCAL_SCAN_DIRS}")
