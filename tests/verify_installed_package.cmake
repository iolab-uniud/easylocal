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

function(easylocal_configure_build_test_consumer name use_core_component use_config_toml_component use_tui_component)
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
        "-DEASYLOCAL_FIND_CONFIG_TOML_COMPONENT=${use_config_toml_component}"
        "-DEASYLOCAL_FIND_TUI_COMPONENT=${use_tui_component}"
    )
    if(DEFINED EASYLOCAL_DEPENDENCY_PREFIX_PATH
            AND NOT "${EASYLOCAL_DEPENDENCY_PREFIX_PATH}" STREQUAL "")
        list(APPEND _configure_command
            "-DCMAKE_PREFIX_PATH=${EASYLOCAL_DEPENDENCY_PREFIX_PATH}")
    endif()
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
if(NOT EXISTS "${_package_dir}/EasyLocalTargets.cmake")
    message(FATAL_ERROR
        "installed Core target export is missing")
endif()

set(_config_toml_targets
    "${_package_dir}/EasyLocalConfigTOMLTargets.cmake")
if(EASYLOCAL_CONFIG_TOML_ENABLED)
    if(NOT EXISTS "${_config_toml_targets}")
        message(FATAL_ERROR
            "ConfigTOML was enabled but its target export is missing")
    endif()
elseif(EXISTS "${_config_toml_targets}")
    message(FATAL_ERROR
        "ConfigTOML target export leaked into a Core-only installation")
endif()

set(_tui_targets "${_package_dir}/EasyLocalTUITargets.cmake")
if(EASYLOCAL_TUI_ENABLED)
    if(NOT EXISTS "${_tui_targets}")
        message(FATAL_ERROR
            "TUI was enabled but its target export is missing")
    endif()
elseif(EXISTS "${_tui_targets}")
    message(FATAL_ERROR
        "TUI target export leaked into a Core-only installation")
endif()

set(_tui_header
    "${_install_prefix}/include/easylocal/tui/tester.hpp")
if(EASYLOCAL_TUI_ENABLED)
    if(NOT EXISTS "${_tui_header}")
        message(FATAL_ERROR
            "TUI was enabled but its installed header is missing")
    endif()
    if(EASYLOCAL_TUI_BUNDLED)
        set(_bundled_ftxui_config
            "${_install_prefix}/${EASYLOCAL_INSTALL_LIBDIR}/cmake/ftxui/ftxui-config.cmake")
        if(NOT EXISTS "${_bundled_ftxui_config}")
            message(FATAL_ERROR
                "TUI fetched FTXUI but its installed package config is missing")
        endif()
    endif()
else()
    if(EXISTS "${_tui_header}")
        message(FATAL_ERROR
            "TUI header leaked into a Core-only installation")
    endif()
endif()

set(_toml_header
    "${_install_prefix}/include/easylocal/config/toml.hpp")
set(_bundled_toml_header
    "${_install_prefix}/include/easylocal/third_party/tomlplusplus/toml++/toml.hpp")
if(EASYLOCAL_CONFIG_TOML_ENABLED)
    if(NOT EXISTS "${_toml_header}")
        message(FATAL_ERROR
            "ConfigTOML was enabled but its installed header is missing")
    endif()
    if(EASYLOCAL_CONFIG_TOML_BUNDLED)
        if(NOT EXISTS "${_bundled_toml_header}")
            message(FATAL_ERROR
                "ConfigTOML fetched toml++ but its private installed headers are missing")
        endif()
    elseif(EXISTS "${_bundled_toml_header}")
        message(FATAL_ERROR
            "system ConfigTOML installation unexpectedly bundled toml++ headers")
    endif()
else()
    if(EXISTS "${_toml_header}")
        message(FATAL_ERROR
            "ConfigTOML header leaked into a Core-only installation")
    endif()
    if(EXISTS "${_bundled_toml_header}")
        message(FATAL_ERROR
            "bundled toml++ headers leaked into a Core-only installation")
    endif()
endif()

easylocal_configure_build_test_consumer(legacy OFF OFF OFF)
easylocal_configure_build_test_consumer(core-component ON OFF OFF)

if(EASYLOCAL_CONFIG_TOML_ENABLED)
    easylocal_configure_build_test_consumer(config-toml ON ON OFF)
endif()

if(EASYLOCAL_TUI_ENABLED)
    easylocal_configure_build_test_consumer(tui ON OFF ON)
endif()

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
if(NOT _missing_component_output MATCHES "ConfigYAML")
    message(FATAL_ERROR
        "missing-component diagnostic did not name ConfigYAML:\n${_missing_component_output}")
endif()
