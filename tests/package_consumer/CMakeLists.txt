cmake_minimum_required(VERSION 3.25)

project(EasyLocalInstalledPackageConsumer LANGUAGES CXX)

if(NOT DEFINED EASYLOCAL_EXPECTED_VERSION)
    message(FATAL_ERROR "EASYLOCAL_EXPECTED_VERSION must be provided")
endif()

if(EASYLOCAL_FIND_MISSING_COMPONENT)
    find_package(
        EasyLocal
        "${EASYLOCAL_EXPECTED_VERSION}"
        EXACT
        CONFIG
        REQUIRED
        COMPONENTS ConfigYAML
    )
elseif(EASYLOCAL_FIND_REST_COMPONENT)
    find_package(
        EasyLocal
        "${EASYLOCAL_EXPECTED_VERSION}"
        EXACT
        CONFIG
        REQUIRED
        COMPONENTS Core REST
    )
elseif(EASYLOCAL_FIND_TUI_COMPONENT)
    find_package(
        EasyLocal
        "${EASYLOCAL_EXPECTED_VERSION}"
        EXACT
        CONFIG
        REQUIRED
        COMPONENTS Core TUI
    )
elseif(EASYLOCAL_FIND_CONFIG_TOML_COMPONENT)
    find_package(
        EasyLocal
        "${EASYLOCAL_EXPECTED_VERSION}"
        EXACT
        CONFIG
        REQUIRED
        COMPONENTS Core ConfigTOML
    )
elseif(EASYLOCAL_FIND_CORE_COMPONENT)
    find_package(
        EasyLocal
        "${EASYLOCAL_EXPECTED_VERSION}"
        EXACT
        CONFIG
        REQUIRED
        COMPONENTS Core
    )
else()
    find_package(
        EasyLocal
        "${EASYLOCAL_EXPECTED_VERSION}"
        EXACT
        CONFIG
        REQUIRED
    )
endif()

foreach(_required_target IN ITEMS EasyLocal::Core EasyLocal::EasyLocal)
    if(NOT TARGET ${_required_target})
        message(FATAL_ERROR
            "installed package did not provide ${_required_target}")
    endif()
endforeach()

get_target_property(
    _easylocal_compile_features
    EasyLocal::Core
    INTERFACE_COMPILE_FEATURES
)
list(FIND _easylocal_compile_features cxx_std_23 _easylocal_cxx23_index)
if(_easylocal_cxx23_index EQUAL -1)
    message(FATAL_ERROR
        "EasyLocal::Core does not propagate the cxx_std_23 requirement")
endif()

get_target_property(
    _easylocal_include_dirs
    EasyLocal::Core
    INTERFACE_INCLUDE_DIRECTORIES
)
if(NOT _easylocal_include_dirs)
    message(FATAL_ERROR
        "EasyLocal::Core does not expose installed include directories")
endif()

if(NOT DEFINED EASYLOCAL_EXPECTED_INSTALL_PREFIX)
    message(FATAL_ERROR "EASYLOCAL_EXPECTED_INSTALL_PREFIX must be provided")
endif()
cmake_path(
    NORMAL_PATH EASYLOCAL_EXPECTED_INSTALL_PREFIX
    OUTPUT_VARIABLE _expected_install_prefix
)
foreach(_include_dir IN LISTS _easylocal_include_dirs)
    if(NOT IS_ABSOLUTE "${_include_dir}")
        message(FATAL_ERROR
            "installed EasyLocal target exposes a non-absolute include path: ${_include_dir}")
    endif()
    cmake_path(NORMAL_PATH _include_dir OUTPUT_VARIABLE _normalized_include_dir)
    cmake_path(
        IS_PREFIX _expected_install_prefix
        "${_normalized_include_dir}"
        NORMALIZE
        _is_installed_include
    )
    if(NOT _is_installed_include)
        message(FATAL_ERROR
            "installed EasyLocal target leaks an include path outside the installation prefix: ${_include_dir}")
    endif()
endforeach()

get_target_property(
    _core_link_libraries
    EasyLocal::Core
    INTERFACE_LINK_LIBRARIES
)
if(_core_link_libraries)
    message(FATAL_ERROR
        "EasyLocal::Core must be dependency-free, but links: ${_core_link_libraries}")
endif()

get_target_property(
    _compat_link_libraries
    EasyLocal::EasyLocal
    INTERFACE_LINK_LIBRARIES
)
if(NOT _compat_link_libraries STREQUAL "EasyLocal::Core")
    message(FATAL_ERROR
        "EasyLocal::EasyLocal must be a Core-only compatibility facade, got: ${_compat_link_libraries}")
endif()

add_executable(
    easylocal_installed_package_consumer
    main.cpp
)
target_link_libraries(
    easylocal_installed_package_consumer
    PRIVATE
        EasyLocal::Core
)


if(EASYLOCAL_FIND_TUI_COMPONENT)
    if(NOT TARGET EasyLocal::TUI)
        message(FATAL_ERROR
            "installed TUI component did not provide EasyLocal::TUI")
    endif()
    target_link_libraries(
        easylocal_installed_package_consumer
        PRIVATE
            EasyLocal::TUI
    )
    target_compile_definitions(
        easylocal_installed_package_consumer
        PRIVATE
            EASYLOCAL_TEST_TUI=1
    )
endif()

if(EASYLOCAL_FIND_CONFIG_TOML_COMPONENT)
    if(NOT TARGET EasyLocal::ConfigTOML)
        message(FATAL_ERROR
            "installed ConfigTOML component did not provide EasyLocal::ConfigTOML")
    endif()
    target_link_libraries(
        easylocal_installed_package_consumer
        PRIVATE
            EasyLocal::ConfigTOML
    )
    target_compile_definitions(
        easylocal_installed_package_consumer
        PRIVATE
            EASYLOCAL_TEST_CONFIG_TOML=1
    )
endif()

if(EASYLOCAL_FIND_REST_COMPONENT)
    if(NOT TARGET EasyLocal::REST)
        message(FATAL_ERROR
            "installed REST component did not provide EasyLocal::REST")
    endif()
    target_link_libraries(
        easylocal_installed_package_consumer
        PRIVATE
            EasyLocal::REST
    )
    target_compile_definitions(
        easylocal_installed_package_consumer
        PRIVATE
            EASYLOCAL_TEST_REST=1
    )
endif()

# Optional targets are loaded only when their package component is requested.
# This keeps a Core-only consumer independent from adapter dependencies even
# when the producer installation happens to contain those adapters.
if(NOT EASYLOCAL_FIND_CONFIG_TOML_COMPONENT AND TARGET EasyLocal::ConfigTOML)
    message(FATAL_ERROR
        "ConfigTOML target leaked into a consumer that requested only Core")
endif()
if(NOT EASYLOCAL_FIND_TUI_COMPONENT AND TARGET EasyLocal::TUI)
    message(FATAL_ERROR
        "TUI target leaked into a consumer that requested only Core")
endif()
if(NOT EASYLOCAL_FIND_REST_COMPONENT AND TARGET EasyLocal::REST)
    message(FATAL_ERROR
        "REST target leaked into a consumer that requested only Core")
endif()

enable_testing()
add_test(
    NAME easylocal.installed-package-consumer.runs
    COMMAND easylocal_installed_package_consumer
)
