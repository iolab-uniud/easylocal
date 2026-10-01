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

# EasyLocal::Core is the canonical dependency-free public target.
# EasyLocal::EasyLocal remains as a compatibility facade for existing users.
add_library(easylocal_core INTERFACE)
add_library(EasyLocal::Core ALIAS easylocal_core)
set_target_properties(
    easylocal_core
    PROPERTIES
        EXPORT_NAME Core
)

target_compile_features(
    easylocal_core
    INTERFACE
        cxx_std_23
)

target_include_directories(
    easylocal_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_library(easylocal INTERFACE)
add_library(EasyLocal::EasyLocal ALIAS easylocal)
set_target_properties(
    easylocal
    PROPERTIES
        EXPORT_NAME EasyLocal
)
target_link_libraries(
    easylocal
    INTERFACE
        easylocal_core
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

option(
    EASYLOCAL_FETCH_DEPENDENCIES
    "Allow optional EasyLocal adapters to fetch missing third-party dependencies"
    OFF
)

option(
    EASYLOCAL_ENABLE_CONFIG_TOML
    "Build the optional toml++ configuration adapter"
    OFF
)

option(
    EASYLOCAL_ENABLE_TUI
    "Build the optional FTXUI terminal tester frontend"
    OFF
)

option(
    EASYLOCAL_ENABLE_REST
    "Build the optional Crow REST application adapter"
    OFF
)

if(EASYLOCAL_ENABLE_CONFIG_TOML)
    set(EASYLOCAL_CONFIG_TOML_BUNDLED FALSE)
    find_package(tomlplusplus 3.4 CONFIG QUIET)

    if(NOT tomlplusplus_FOUND)
        if(EASYLOCAL_FETCH_DEPENDENCIES)
            include(FetchContent)
            set(TOMLPLUSPLUS_BUILD_MODULES OFF CACHE BOOL "" FORCE)
            FetchContent_Declare(
                tomlplusplus
                GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
                GIT_TAG v3.4.0
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(tomlplusplus)
            set(EASYLOCAL_CONFIG_TOML_BUNDLED TRUE)
        else()
            message(FATAL_ERROR
                "EasyLocal ConfigTOML requires tomlplusplus >= 3.4. "
                "Install tomlplusplus or reconfigure with "
                "EASYLOCAL_FETCH_DEPENDENCIES=ON to allow FetchContent.")
        endif()
    endif()

    add_library(easylocal_config_toml INTERFACE)
    add_library(EasyLocal::ConfigTOML ALIAS easylocal_config_toml)
    set_target_properties(
        easylocal_config_toml
        PROPERTIES
            EXPORT_NAME ConfigTOML
    )
    target_link_libraries(
        easylocal_config_toml
        INTERFACE
            easylocal_core
    )

    if(EASYLOCAL_CONFIG_TOML_BUNDLED)
        # Keep the FetchContent target a build-tree implementation detail.
        # The installed adapter uses the private header copy below instead.
        target_link_libraries(
            easylocal_config_toml
            INTERFACE
                $<BUILD_INTERFACE:tomlplusplus::tomlplusplus>
        )
        target_include_directories(
            easylocal_config_toml
            INTERFACE
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/easylocal/third_party/tomlplusplus>
        )
    else()
        target_link_libraries(
            easylocal_config_toml
            INTERFACE
                tomlplusplus::tomlplusplus
        )
    endif()
endif()

if(EASYLOCAL_ENABLE_TUI)
    set(EASYLOCAL_TUI_BUNDLED FALSE)
    find_package(ftxui 7 CONFIG QUIET)

    if(NOT ftxui_FOUND)
        if(EASYLOCAL_FETCH_DEPENDENCIES)
            include(FetchContent)
            set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(FTXUI_BUILD_MODULES OFF CACHE BOOL "" FORCE)
            set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(FTXUI_BUILD_TESTS_FUZZER OFF CACHE BOOL "" FORCE)
            set(FTXUI_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
            FetchContent_Declare(
                ftxui
                GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
                GIT_TAG v7.0.3
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(ftxui)
            set(EASYLOCAL_TUI_BUNDLED TRUE)
        else()
            message(FATAL_ERROR
                "EasyLocal TUI requires FTXUI >= 7.0. "
                "Install FTXUI or reconfigure with "
                "EASYLOCAL_FETCH_DEPENDENCIES=ON to allow FetchContent.")
        endif()
    endif()

    add_library(easylocal_tui INTERFACE)
    add_library(EasyLocal::TUI ALIAS easylocal_tui)
    set_target_properties(
        easylocal_tui
        PROPERTIES
            EXPORT_NAME TUI
    )
    target_link_libraries(
        easylocal_tui
        INTERFACE
            easylocal_core
    )

    if(EASYLOCAL_TUI_BUNDLED)
        # The fetched FTXUI target belongs to the build tree. Its own install
        # rules provide ftxui::ftxui to consumers of the installed TUI target.
        target_link_libraries(
            easylocal_tui
            INTERFACE
                $<BUILD_INTERFACE:ftxui::ftxui>
                $<INSTALL_INTERFACE:ftxui::ftxui>
        )
    else()
        target_link_libraries(
            easylocal_tui
            INTERFACE
                ftxui::ftxui
        )
    endif()
endif()

if(EASYLOCAL_ENABLE_REST)
    set(EASYLOCAL_REST_BUNDLED FALSE)
    set(EASYLOCAL_REST_ASIO_BUNDLED FALSE)
    find_package(Crow 1.3 CONFIG QUIET)

    if(NOT Crow_FOUND)
        if(EASYLOCAL_FETCH_DEPENDENCIES)
            include(FetchContent)

            # Crow uses standalone Asio by default but does not fetch it.
            # Keep EASYLOCAL_FETCH_DEPENDENCIES self-contained for REST too.
            find_path(EASYLOCAL_ASIO_INCLUDE_DIR asio.hpp)
            if(NOT EASYLOCAL_ASIO_INCLUDE_DIR)
                FetchContent_Declare(
                    easylocal_asio
                    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
                    GIT_TAG asio-1-38-2
                    GIT_SHALLOW TRUE
                    # Asio is consumed as headers only.  Use a deliberately
                    # nonexistent source subdirectory so MakeAvailable() only
                    # performs population and never adds Asio's own project.
                    SOURCE_SUBDIR _easylocal_headers_only
                )
                FetchContent_MakeAvailable(easylocal_asio)
                # asio/include is a symlink to the top-level include/ since
                # Asio 1.38; resolve it so install(DIRECTORY) copies headers
                # rather than a dangling relative link.
                file(
                    REAL_PATH
                    "${easylocal_asio_SOURCE_DIR}/asio/include"
                    EASYLOCAL_ASIO_INCLUDE_DIR
                )
                set(
                    ASIO_INCLUDE_DIR
                    "${EASYLOCAL_ASIO_INCLUDE_DIR}"
                    CACHE PATH
                    "Standalone Asio headers used by bundled Crow"
                    FORCE
                )
                set(EASYLOCAL_REST_ASIO_BUNDLED TRUE)
            endif()

            set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(CROW_BUILD_FUZZER OFF CACHE BOOL "" FORCE)
            set(CROW_ENABLE_SSL OFF CACHE BOOL "" FORCE)
            set(CROW_ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
            set(CROW_INSTALL ON CACHE BOOL "" FORCE)
            FetchContent_Declare(
                Crow
                GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
                GIT_TAG v1.3.3
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(Crow)
            set(EASYLOCAL_REST_BUNDLED TRUE)
        else()
            message(FATAL_ERROR
                "EasyLocal REST requires Crow >= 1.3. "
                "Install Crow or reconfigure with "
                "EASYLOCAL_FETCH_DEPENDENCIES=ON to allow FetchContent.")
        endif()
    endif()

    add_library(easylocal_rest INTERFACE)
    add_library(EasyLocal::REST ALIAS easylocal_rest)
    set_target_properties(
        easylocal_rest
        PROPERTIES
            EXPORT_NAME REST
    )
    target_link_libraries(
        easylocal_rest
        INTERFACE
            easylocal_core
            Crow::Crow
    )
endif()

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

install(
    TARGETS
        easylocal_core
        easylocal
    EXPORT EasyLocalTargets
)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    # Optional adapters are installed by their own components below.
    PATTERN "easylocal/adapters" EXCLUDE
)

if(EASYLOCAL_ENABLE_CONFIG_TOML)
    install(
        TARGETS easylocal_config_toml
        EXPORT EasyLocalConfigTOMLTargets
    )
    install(
        EXPORT EasyLocalConfigTOMLTargets
        FILE EasyLocalConfigTOMLTargets.cmake
        NAMESPACE EasyLocal::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EasyLocal"
    )
    install(
        FILES include/easylocal/adapters/toml.hpp
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/adapters"
    )

    if(EASYLOCAL_CONFIG_TOML_BUNDLED)
        # toml++ only defines install rules when it is the top-level project.
        # A FetchContent dependency therefore needs a private installed copy of
        # its headers so the installed ConfigTOML component remains relocatable.
        install(
            DIRECTORY "${tomlplusplus_SOURCE_DIR}/include/"
            DESTINATION
                "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/third_party/tomlplusplus"
        )
    endif()
endif()

if(EASYLOCAL_ENABLE_TUI)
    install(
        TARGETS easylocal_tui
        EXPORT EasyLocalTUITargets
    )
    install(
        EXPORT EasyLocalTUITargets
        FILE EasyLocalTUITargets.cmake
        NAMESPACE EasyLocal::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EasyLocal"
    )
    install(
        DIRECTORY include/easylocal/adapters/tui/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/adapters/tui"
    )
endif()

if(EASYLOCAL_ENABLE_REST)
    install(
        TARGETS easylocal_rest
        EXPORT EasyLocalRESTTargets
    )
    install(
        EXPORT EasyLocalRESTTargets
        FILE EasyLocalRESTTargets.cmake
        NAMESPACE EasyLocal::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EasyLocal"
    )
    install(
        FILES include/easylocal/adapters/rest.hpp
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/adapters"
    )
    install(
        DIRECTORY include/easylocal/adapters/rest/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/adapters/rest"
    )

    if(EASYLOCAL_REST_ASIO_BUNDLED)
        # Crow installs its package metadata but expects standalone Asio to be
        # discoverable. Keep a private header copy for relocatable installs.
        install(
            DIRECTORY "${EASYLOCAL_ASIO_INCLUDE_DIR}/"
            DESTINATION
                "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/third_party/asio"
        )
    endif()
endif()

if(EASYLOCAL_ENABLE_CONFIG_TOML)
    set(EASYLOCAL_CONFIG_TOML_AVAILABLE TRUE)
else()
    set(EASYLOCAL_CONFIG_TOML_AVAILABLE FALSE)
    set(EASYLOCAL_CONFIG_TOML_BUNDLED FALSE)
endif()

if(EASYLOCAL_ENABLE_TUI)
    set(EASYLOCAL_TUI_AVAILABLE TRUE)
else()
    set(EASYLOCAL_TUI_AVAILABLE FALSE)
    set(EASYLOCAL_TUI_BUNDLED FALSE)
endif()

if(EASYLOCAL_ENABLE_REST)
    set(EASYLOCAL_REST_AVAILABLE TRUE)
else()
    set(EASYLOCAL_REST_AVAILABLE FALSE)
    set(EASYLOCAL_REST_BUNDLED FALSE)
    set(EASYLOCAL_REST_ASIO_BUNDLED FALSE)
endif()

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
