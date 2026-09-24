cmake_minimum_required(VERSION 3.25)

project(EasyLocalInstalledPackageConsumer LANGUAGES CXX)

if(NOT DEFINED EASYLOCAL_EXPECTED_VERSION)
    message(FATAL_ERROR "EASYLOCAL_EXPECTED_VERSION must be provided")
endif()

find_package(
    EasyLocal
    "${EASYLOCAL_EXPECTED_VERSION}"
    EXACT
    CONFIG
    REQUIRED
)

if(NOT TARGET EasyLocal::EasyLocal)
    message(FATAL_ERROR "installed package did not provide EasyLocal::EasyLocal")
endif()

get_target_property(
    _easylocal_compile_features
    EasyLocal::EasyLocal
    INTERFACE_COMPILE_FEATURES
)
list(FIND _easylocal_compile_features cxx_std_23 _easylocal_cxx23_index)
if(_easylocal_cxx23_index EQUAL -1)
    message(FATAL_ERROR
        "EasyLocal::EasyLocal does not propagate the cxx_std_23 requirement")
endif()

get_target_property(
    _easylocal_include_dirs
    EasyLocal::EasyLocal
    INTERFACE_INCLUDE_DIRECTORIES
)
if(NOT _easylocal_include_dirs)
    message(FATAL_ERROR
        "EasyLocal::EasyLocal does not expose installed include directories")
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

add_executable(
    easylocal_installed_package_consumer
    main.cpp
)
target_link_libraries(
    easylocal_installed_package_consumer
    PRIVATE
        EasyLocal::EasyLocal
)

enable_testing()
add_test(
    NAME easylocal.installed-package-consumer.runs
    COMMAND easylocal_installed_package_consumer
)
