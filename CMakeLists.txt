cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" EASYLOCAL_VERSION)
string(STRIP "${EASYLOCAL_VERSION}" EASYLOCAL_VERSION)

# VERSION holds the full SemVer string (4.0.0 or 4.0.0-alpha.1): project()
# takes only its numeric part, the pre-release goes in its own variable.
if(NOT EASYLOCAL_VERSION MATCHES
        "^([0-9]+\\.[0-9]+\\.[0-9]+)(-([0-9A-Za-z.-]+))?$")
    message(FATAL_ERROR
        "VERSION must contain MAJOR.MINOR.PATCH[-PRERELEASE], got '${EASYLOCAL_VERSION}'")
endif()
set(EASYLOCAL_VERSION_NUMBER "${CMAKE_MATCH_1}")
set(EASYLOCAL_VERSION_PRERELEASE "${CMAKE_MATCH_3}")

project(
    EasyLocal
    VERSION "${EASYLOCAL_VERSION_NUMBER}"
    DESCRIPTION "A modern header-only C++ framework for local search"
    HOMEPAGE_URL "https://github.com/iolab-uniud/easylocal"
    LANGUAGES CXX
)

# EasyLocal::Core is the dependency-free public target.
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

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# The optional components (ConfigTOML, TUI, REST): each finds its dependency,
# or fetches it with EASYLOCAL_FETCH_DEPENDENCIES, then adds an adapter target
# on top of the core.

# Stops the configuration: the component's dependency is neither installed nor
# allowed to be fetched.
function(easylocal_missing_dependency component requirement package)
    message(FATAL_ERROR
        "EasyLocal ${component} requires ${requirement}. "
        "Install ${package} or reconfigure with "
        "EASYLOCAL_FETCH_DEPENDENCIES=ON to allow FetchContent.")
endfunction()

# The INTERFACE target of a component, on top of the core, exported as
# EasyLocal::<component>.
function(easylocal_add_adapter target component)
    add_library(${target} INTERFACE)
    add_library(EasyLocal::${component} ALIAS ${target})
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${component})
    target_link_libraries(${target} INTERFACE easylocal_core)
endfunction()

# Installs a component: its target, its export file and its headers under
# easylocal/adapters (FILES, and the DIRECTORIES of adapters/).
function(easylocal_install_adapter target component)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "FILES;DIRECTORIES")
    install(TARGETS ${target} EXPORT EasyLocal${component}Targets)
    install(
        EXPORT EasyLocal${component}Targets
        FILE EasyLocal${component}Targets.cmake
        NAMESPACE EasyLocal::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EasyLocal"
    )
    install(
        FILES ${arg_FILES}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/adapters"
    )
    foreach(directory IN LISTS arg_DIRECTORIES)
        install(
            DIRECTORY include/easylocal/adapters/${directory}/
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/easylocal/adapters/${directory}"
        )
    endforeach()
endfunction()

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
    EASYLOCAL_STRICT_WARNINGS
    "Compile EasyLocal's own tests and examples with strict warnings"
    ${PROJECT_IS_TOP_LEVEL}
)

option(
    EASYLOCAL_WARNINGS_AS_ERRORS
    "Treat warnings in EasyLocal's own tests and examples as errors"
    OFF
)

option(
    EASYLOCAL_ENABLE_COVERAGE
    "Instrument EasyLocal's own tests and examples for code coverage (GCC/Clang)"
    OFF
)

# Strict warnings and coverage instrumentation apply only to EasyLocal's own
# targets in the calling directory; consumers of the header-only library are
# never affected. Coverage of the headers comes from the translation units
# that instantiate them.
function(easylocal_configure_own_targets)
    if(EASYLOCAL_ENABLE_COVERAGE AND NOT MSVC)
        add_compile_options(--coverage -O0)
        add_link_options(--coverage)
    endif()
    if(NOT EASYLOCAL_STRICT_WARNINGS)
        return()
    endif()
    if(MSVC)
        add_compile_options(/W4 /permissive-)
        if(EASYLOCAL_WARNINGS_AS_ERRORS)
            add_compile_options(/WX)
        endif()
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
        if(EASYLOCAL_WARNINGS_AS_ERRORS)
            add_compile_options(-Werror)
        endif()
    endif()
endfunction()

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
                SYSTEM  # its warnings are not ours
            )
            FetchContent_MakeAvailable(tomlplusplus)
            set(EASYLOCAL_CONFIG_TOML_BUNDLED TRUE)
        else()
            easylocal_missing_dependency(
                ConfigTOML "tomlplusplus >= 3.4" tomlplusplus)
        endif()
    endif()

    easylocal_add_adapter(easylocal_config_toml ConfigTOML)

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
                SYSTEM  # its warnings are not ours
            )
            FetchContent_MakeAvailable(ftxui)
            set(EASYLOCAL_TUI_BUNDLED TRUE)
        else()
            easylocal_missing_dependency(TUI "FTXUI >= 7.0" FTXUI)
        endif()
    endif()

    easylocal_add_adapter(easylocal_tui TUI)

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
        # Crow 1.3 packages (Homebrew, a plain `cmake --install`) ship no
        # CrowConfigVersion.cmake, so a versioned request rejects them.
        find_package(Crow CONFIG QUIET)
        if(Crow_FOUND)
            message(STATUS "EasyLocal REST: Crow found without version information; assuming >= 1.3")
        endif()
    endif()

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
                SYSTEM  # its warnings are not ours
            )
            FetchContent_MakeAvailable(Crow)
            set(EASYLOCAL_REST_BUNDLED TRUE)
        else()
            easylocal_missing_dependency(REST "Crow >= 1.3" Crow)
        endif()
    endif()

    easylocal_add_adapter(easylocal_rest REST)
    target_link_libraries(easylocal_rest INTERFACE Crow::Crow)
endif()

# The examples are deliberately outside the public EasyLocal API.
# Its model targets are also consumed by the tests (and by easylocal-benchmarks,
# which compiles the examples from a checkout), so make them
# available whenever one of those project facilities is requested.
if(EASYLOCAL_BUILD_EXAMPLES OR EASYLOCAL_BUILD_TESTS)
    add_subdirectory(examples)
endif()

if(EASYLOCAL_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

install(
    TARGETS
        easylocal_core
    EXPORT EasyLocalTargets
)

install(
    FILES LICENSE
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/easylocal"
)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    # Optional adapters are installed by their own components below.
    PATTERN "easylocal/adapters" EXCLUDE
)

if(EASYLOCAL_ENABLE_CONFIG_TOML)
    easylocal_install_adapter(
        easylocal_config_toml ConfigTOML
        FILES include/easylocal/adapters/toml.hpp
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
    easylocal_install_adapter(
        easylocal_tui TUI
        FILES include/easylocal/adapters/tui.hpp
        DIRECTORIES tui
    )
endif()

if(EASYLOCAL_ENABLE_REST)
    easylocal_install_adapter(
        easylocal_rest REST
        FILES include/easylocal/adapters/rest.hpp
        DIRECTORIES rest
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
