cmake_minimum_required(VERSION 3.25)

# Builds and runs the quick start as a student downloads it (main.cpp and
# standalone/CMakeLists.txt in one folder), and checks what it prints. With
# EASYLOCAL_PREFIX it finds the EasyLocal installed there, through
# CMAKE_PREFIX_PATH, as docs/quick-start.md says; with EASYLOCAL_SOURCE_DIR
# its find_package line is replaced by add_subdirectory, the alternative the
# page gives.

foreach(_required IN ITEMS
        EASYLOCAL_QUICKSTART_DIR
        EASYLOCAL_WORK_DIR
        EASYLOCAL_CXX_COMPILER
        EASYLOCAL_GENERATOR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} must be provided")
    endif()
endforeach()

function(easylocal_quickstart_run description)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE _result COMMAND_ECHO STDOUT)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "${description} failed with exit code ${_result}")
    endif()
endfunction()

set(_source "${EASYLOCAL_WORK_DIR}/src")
set(_build "${EASYLOCAL_WORK_DIR}/build")
file(REMOVE_RECURSE "${EASYLOCAL_WORK_DIR}")
file(MAKE_DIRECTORY "${_source}")
file(COPY "${EASYLOCAL_QUICKSTART_DIR}/main.cpp" DESTINATION "${_source}")
file(READ "${EASYLOCAL_QUICKSTART_DIR}/standalone/CMakeLists.txt" _lists)

set(_configure
    "${CMAKE_COMMAND}" -S "${_source}" -B "${_build}" -G "${EASYLOCAL_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${EASYLOCAL_CXX_COMPILER}")
if(DEFINED EASYLOCAL_SOURCE_DIR AND NOT "${EASYLOCAL_SOURCE_DIR}" STREQUAL "")
    set(_find "find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)")
    string(FIND "${_lists}" "${_find}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "standalone/CMakeLists.txt has no '${_find}' line to replace")
    endif()
    string(REPLACE "${_find}" "add_subdirectory(\"${EASYLOCAL_SOURCE_DIR}\" easylocal)"
        _lists "${_lists}")
elseif(DEFINED EASYLOCAL_PREFIX AND NOT "${EASYLOCAL_PREFIX}" STREQUAL "")
    list(APPEND _configure "-DCMAKE_PREFIX_PATH=${EASYLOCAL_PREFIX}")
else()
    message(FATAL_ERROR "EASYLOCAL_PREFIX or EASYLOCAL_SOURCE_DIR must be provided")
endif()
file(WRITE "${_source}/CMakeLists.txt" "${_lists}")

foreach(_option IN ITEMS MAKE_PROGRAM BUILD_TYPE)
    if(DEFINED EASYLOCAL_${_option} AND NOT "${EASYLOCAL_${_option}}" STREQUAL "")
        list(APPEND _configure "-DCMAKE_${_option}=${EASYLOCAL_${_option}}")
    endif()
endforeach()
foreach(_option IN ITEMS GENERATOR_PLATFORM:-A GENERATOR_TOOLSET:-T)
    string(REPLACE ":" ";" _option "${_option}")
    list(GET _option 0 _name)
    list(GET _option 1 _flag)
    if(DEFINED EASYLOCAL_${_name} AND NOT "${EASYLOCAL_${_name}}" STREQUAL "")
        list(APPEND _configure "${_flag}" "${EASYLOCAL_${_name}}")
    endif()
endforeach()
easylocal_quickstart_run("configuring the quick start" ${_configure})

set(_build_command "${CMAKE_COMMAND}" --build "${_build}")
if(DEFINED EASYLOCAL_CONFIG AND NOT "${EASYLOCAL_CONFIG}" STREQUAL "")
    list(APPEND _build_command --config "${EASYLOCAL_CONFIG}")
endif()
easylocal_quickstart_run("building the quick start" ${_build_command})

file(GLOB_RECURSE _programs "${_build}/tsp" "${_build}/tsp.exe")
list(FILTER _programs EXCLUDE REGEX "/CMakeFiles/")
if(NOT _programs)
    message(FATAL_ERROR "the quick start program tsp was not built in ${_build}")
endif()
list(GET _programs 0 _program)
execute_process(
    COMMAND "${_program}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _output)
if(NOT _result EQUAL 0 OR NOT _output MATCHES "length 26 after 14 evaluations")
    message(FATAL_ERROR
        "the quick start exited with ${_result}, printing:\n${_output}")
endif()
message(STATUS "the quick start printed:\n${_output}")
