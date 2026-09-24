cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" EASYLOCAL_VERSION)
string(STRIP "${EASYLOCAL_VERSION}" EASYLOCAL_VERSION)

if(NOT EASYLOCAL_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR
        "VERSION must contain MAJOR.MINOR.PATCH, got '${EASYLOCAL_VERSION}'")
endif()

project(
    EasyLocal
    VERSION "${EASYLOCAL_VERSION}"
    DESCRIPTION "A modern header-only C++ framework for local search"
    LANGUAGES CXX
)

add_library(easylocal INTERFACE)
add_library(EasyLocal::EasyLocal ALIAS easylocal)

set_target_properties(
    easylocal
    PROPERTIES
        EXPORT_NAME EasyLocal
)

target_compile_features(
    easylocal
    INTERFACE
        cxx_std_23
)

target_include_directories(
    easylocal
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

option(
    EASYLOCAL_BUILD_EXAMPLES
    "Build EasyLocal examples"
    ${PROJECT_IS_TOP_LEVEL}
)

option(
    EASYLOCAL_BUILD_TESTS
    "Build the EasyLocal test suite"
    ${PROJECT_IS_TOP_LEVEL}
)

option(
    EASYLOCAL_BUILD_BENCHMARKS
    "Build opt-in EasyLocal performance benchmarks"
    OFF
)

# The concrete MWE is deliberately outside the public EasyLocal++ API.
# Its model targets are also consumed by tests and benchmarks, so make them
# available whenever one of those project facilities is requested.
if(EASYLOCAL_BUILD_EXAMPLES OR EASYLOCAL_BUILD_TESTS OR EASYLOCAL_BUILD_BENCHMARKS)
    add_subdirectory(examples)
endif()

if(EASYLOCAL_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

if(EASYLOCAL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks/neighborhood_traversal)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS easylocal
    EXPORT EasyLocalTargets
)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EasyLocalConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/EasyLocalConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EasyLocal"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/EasyLocalConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    EXPORT EasyLocalTargets
    FILE EasyLocalTargets.cmake
    NAMESPACE EasyLocal::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EasyLocal"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/EasyLocalConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/EasyLocalConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EasyLocal"
)
