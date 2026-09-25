cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
        EASYLOCAL_BUILD_DIR
        EASYLOCAL_CONSUMER_SOURCE_DIR
        EASYLOCAL_CTEST_COMMAND
        EASYLOCAL_CXX_COMPILER
        EASYLOCAL_GENERATOR
        EASYLOCAL_INSTALL_LIBDIR
        EASYLOCAL_VERSION
        EASYLOCAL_WORK_DIR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} must be provided")
    endif()
endforeach()

function(easylocal_run_checked description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE _result
        COMMAND_ECHO STDOUT
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "${description} failed with exit code ${_result}")
    endif()
endfunction()

function(easylocal_configure_build_test_consumer name use_core_component)
    set(_consumer_build_dir "${EASYLOCAL_WORK_DIR}/${name}")
    set(
        _configure_command
        "${CMAKE_COMMAND}"
        -S "${EASYLOCAL_CONSUMER_SOURCE_DIR}"
        -B "${_consumer_build_dir}"
        -G "${EASYLOCAL_GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${EASYLOCAL_CXX_COMPILER}"
        "-DEasyLocal_DIR=${_package_dir}"
        "-DEASYLOCAL_EXPECTED_VERSION=${EASYLOCAL_VERSION}"
        "-DEASYLOCAL_EXPECTED_INSTALL_PREFIX=${_install_prefix}"
        "-DEASYLOCAL_FIND_CORE_COMPONENT=${use_core_component}"
    )
    if(DEFINED EASYLOCAL_GENERATOR_PLATFORM
            AND NOT "${EASYLOCAL_GENERATOR_PLATFORM}" STREQUAL "")
        list(APPEND _configure_command -A "${EASYLOCAL_GENERATOR_PLATFORM}")
    endif()
    if(DEFINED EASYLOCAL_GENERATOR_TOOLSET
            AND NOT "${EASYLOCAL_GENERATOR_TOOLSET}" STREQUAL "")
        list(APPEND _configure_command -T "${EASYLOCAL_GENERATOR_TOOLSET}")
    endif()
    if(DEFINED EASYLOCAL_MAKE_PROGRAM
            AND NOT "${EASYLOCAL_MAKE_PROGRAM}" STREQUAL "")
        list(APPEND _configure_command
            "-DCMAKE_MAKE_PROGRAM=${EASYLOCAL_MAKE_PROGRAM}")
    endif()
    if(DEFINED EASYLOCAL_BUILD_TYPE
            AND NOT "${EASYLOCAL_BUILD_TYPE}" STREQUAL "")
        list(APPEND _configure_command
            "-DCMAKE_BUILD_TYPE=${EASYLOCAL_BUILD_TYPE}")
    endif()
    easylocal_run_checked(
        "configuring the ${name} installed-package consumer"
        ${_configure_command}
    )

    set(
        _build_command
        "${CMAKE_COMMAND}"
        --build "${_consumer_build_dir}"
    )
    if(DEFINED EASYLOCAL_CONFIG AND NOT "${EASYLOCAL_CONFIG}" STREQUAL "")
        list(APPEND _build_command --config "${EASYLOCAL_CONFIG}")
    endif()
    easylocal_run_checked(
        "building the ${name} installed-package consumer"
        ${_build_command}
    )

    set(
        _ctest_command
        "${EASYLOCAL_CTEST_COMMAND}"
        --test-dir "${_consumer_build_dir}"
        --output-on-failure
    )
    if(DEFINED EASYLOCAL_CONFIG AND NOT "${EASYLOCAL_CONFIG}" STREQUAL "")
        list(APPEND _ctest_command -C "${EASYLOCAL_CONFIG}")
    endif()
    easylocal_run_checked(
        "running the ${name} installed-package consumer"
        ${_ctest_command}
    )
endfunction()

file(REMOVE_RECURSE "${EASYLOCAL_WORK_DIR}")

set(_install_prefix "${EASYLOCAL_WORK_DIR}/prefix")
set(_package_dir
    "${_install_prefix}/${EASYLOCAL_INSTALL_LIBDIR}/cmake/EasyLocal")

set(
    _install_command
    "${CMAKE_COMMAND}"
    --install "${EASYLOCAL_BUILD_DIR}"
    --prefix "${_install_prefix}"
)
if(DEFINED EASYLOCAL_CONFIG AND NOT "${EASYLOCAL_CONFIG}" STREQUAL "")
    list(APPEND _install_command --config "${EASYLOCAL_CONFIG}")
endif()
easylocal_run_checked("installing EasyLocal" ${_install_command})

if(NOT EXISTS "${_package_dir}/EasyLocalConfig.cmake")
    message(FATAL_ERROR
        "installed package config not found at ${_package_dir}/EasyLocalConfig.cmake")
endif()

easylocal_configure_build_test_consumer(legacy OFF)
easylocal_configure_build_test_consumer(core-component ON)

set(_missing_component_build_dir "${EASYLOCAL_WORK_DIR}/missing-component")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -S "${EASYLOCAL_CONSUMER_SOURCE_DIR}"
        -B "${_missing_component_build_dir}"
        -G "${EASYLOCAL_GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${EASYLOCAL_CXX_COMPILER}"
        "-DEasyLocal_DIR=${_package_dir}"
        "-DEASYLOCAL_EXPECTED_VERSION=${EASYLOCAL_VERSION}"
        "-DEASYLOCAL_EXPECTED_INSTALL_PREFIX=${_install_prefix}"
        "-DEASYLOCAL_FIND_MISSING_COMPONENT=ON"
    RESULT_VARIABLE _missing_component_result
    OUTPUT_VARIABLE _missing_component_stdout
    ERROR_VARIABLE _missing_component_stderr
)
if(_missing_component_result EQUAL 0)
    message(FATAL_ERROR
        "requesting unavailable EasyLocal::ConfigTOML unexpectedly succeeded")
endif()
string(CONCAT _missing_component_output
    "${_missing_component_stdout}" "${_missing_component_stderr}")
if(NOT _missing_component_output MATCHES "ConfigTOML")
    message(FATAL_ERROR
        "missing-component diagnostic did not name ConfigTOML:\n${_missing_component_output}")
endif()
