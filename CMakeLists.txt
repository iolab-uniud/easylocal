cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build, preferably a CMake preset.")
endif()

project(EasyLocalNext VERSION 0.0.0 LANGUAGES CXX)

# These settings currently apply only to the bootstrap executable.
# There is intentionally no framework/library target yet.
set(CMAKE_CXX_STANDARD "20" CACHE STRING "C++ language mode: 20 or 23")
set_property(CACHE CMAKE_CXX_STANDARD PROPERTY STRINGS 20 23)
if(NOT CMAKE_CXX_STANDARD MATCHES "^(20|23)$")
    message(FATAL_ERROR "This bootstrap accepts only C++20 or C++23.")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    message(FATAL_ERROR "Only GCC, Clang and AppleClang are in scope.")
endif()

# Optional checks used by CI: never infer compiler identity from its filename.
set(EASYLOCAL_EXPECTED_COMPILER_ID "" CACHE STRING "Expected CMake compiler ID")
set(EASYLOCAL_EXPECTED_COMPILER_MAJOR "" CACHE STRING "Expected compiler major")
set(EASYLOCAL_EXPECTED_STDLIB "" CACHE STRING "Expected library: libcxx or libstdcxx")
if(EASYLOCAL_EXPECTED_COMPILER_ID AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL EASYLOCAL_EXPECTED_COMPILER_ID)
    message(FATAL_ERROR
        "Expected ${EASYLOCAL_EXPECTED_COMPILER_ID}, detected ${CMAKE_CXX_COMPILER_ID}.")
endif()
string(REGEX MATCH "^[0-9]+" _compiler_major "${CMAKE_CXX_COMPILER_VERSION}")
if(EASYLOCAL_EXPECTED_COMPILER_MAJOR AND
   NOT _compiler_major STREQUAL EASYLOCAL_EXPECTED_COMPILER_MAJOR)
    message(FATAL_ERROR
        "Expected compiler major ${EASYLOCAL_EXPECTED_COMPILER_MAJOR}, "
        "detected ${CMAKE_CXX_COMPILER_VERSION}.")
endif()
if(EASYLOCAL_EXPECTED_STDLIB AND
   NOT EASYLOCAL_EXPECTED_STDLIB MATCHES "^(libcxx|libstdcxx)$")
    message(FATAL_ERROR "Invalid EASYLOCAL_EXPECTED_STDLIB value.")
endif()

option(EASYLOCAL_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)

include(CTest)
if(BUILD_TESTING)
    add_executable(el_toolchain_smoke tests/toolchain_smoke.cpp)
    target_compile_features(el_toolchain_smoke PRIVATE cxx_std_20)
    target_compile_options(el_toolchain_smoke PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
    if(EASYLOCAL_WARNINGS_AS_ERRORS)
        target_compile_options(el_toolchain_smoke PRIVATE -Werror)
    endif()
    if(EASYLOCAL_EXPECTED_STDLIB STREQUAL "libcxx")
        target_compile_definitions(el_toolchain_smoke PRIVATE EL_EXPECT_LIBCXX=1)
    elseif(EASYLOCAL_EXPECTED_STDLIB STREQUAL "libstdcxx")
        target_compile_definitions(el_toolchain_smoke PRIVATE EL_EXPECT_LIBSTDCXX=1)
    endif()

    add_test(NAME infrastructure.cpp20 COMMAND el_toolchain_smoke)
    add_test(NAME infrastructure.failure-signal COMMAND el_toolchain_smoke --fail)
    # A deliberate nonzero exit must be recognized even in Release builds.
    set_tests_properties(infrastructure.failure-signal PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(infrastructure.cpp20 infrastructure.failure-signal
        PROPERTIES LABELS "infrastructure" TIMEOUT 30)
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" _configuration)
file(WRITE "${CMAKE_BINARY_DIR}/toolchain.txt"
    "CMake: ${CMAKE_VERSION}\n"
    "Generator: ${CMAKE_GENERATOR}\n"
    "System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}\n"
    "Processor: ${CMAKE_SYSTEM_PROCESSOR}\n"
    "Compiler: ${CMAKE_CXX_COMPILER}\n"
    "Compiler ID: ${CMAKE_CXX_COMPILER_ID}\n"
    "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}\n"
    "Requested C++ mode: ${CMAKE_CXX_STANDARD}\n"
    "Build type: ${CMAKE_BUILD_TYPE}\n"
    "Global CXX flags: ${CMAKE_CXX_FLAGS}\n"
    "Configuration CXX flags: ${CMAKE_CXX_FLAGS_${_configuration}}\n"
    "Warnings as errors: ${EASYLOCAL_WARNINGS_AS_ERRORS}\n"
    "Expected standard library: ${EASYLOCAL_EXPECTED_STDLIB}\n"
    "macOS SDK: ${CMAKE_OSX_SYSROOT}\n"
    "macOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}\n"
    "Implicit includes: ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}\n"
    "Implicit link directories: ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}\n")
message(STATUS "Toolchain report: ${CMAKE_BINARY_DIR}/toolchain.txt")
