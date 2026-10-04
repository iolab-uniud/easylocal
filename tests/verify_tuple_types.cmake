cmake_minimum_required(VERSION 3.25)

# The library spells the types of a std::tuple: std::tuple<T>{x}, never
# std::tuple{x}. With one argument, or a pack that may hold one, the deduction
# competes with the copy deduction candidate, and some compilers reject it as
# ambiguous. EASYLOCAL_SCAN_DIRS is the ;-separated list of directories to
# check (default: include of EASYLOCAL_SOURCE_DIR).

if(NOT DEFINED EASYLOCAL_SCAN_DIRS OR EASYLOCAL_SCAN_DIRS STREQUAL "")
    if(NOT DEFINED EASYLOCAL_SOURCE_DIR OR EASYLOCAL_SOURCE_DIR STREQUAL "")
        message(FATAL_ERROR "EASYLOCAL_SOURCE_DIR or EASYLOCAL_SCAN_DIRS must be provided")
    endif()
    set(EASYLOCAL_SCAN_DIRS "${EASYLOCAL_SOURCE_DIR}/include")
endif()

set(_sources)
foreach(_directory IN LISTS EASYLOCAL_SCAN_DIRS)
    file(GLOB_RECURSE _found "${_directory}/*.hpp" "${_directory}/*.cpp")
    list(APPEND _sources ${_found})
endforeach()

set(_violations)
foreach(_source IN LISTS _sources)
    if(_source MATCHES "/third_party/")
        continue()
    endif()
    file(STRINGS "${_source}" _lines)
    set(_number 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _number "${_number} + 1")
        if(_line MATCHES "std::tuple[ \t]*[({]")
            list(APPEND _violations "${_source}:${_number}")
        endif()
    endforeach()
endforeach()

if(_violations)
    list(JOIN _violations "\n  " _report)
    message(FATAL_ERROR
        "std::tuple deduces its types; spell them, std::tuple<T>{x}:\n  "
        "${_report}")
endif()
message(STATUS "No deduced std::tuple in ${EASYLOCAL_SCAN_DIRS}")
