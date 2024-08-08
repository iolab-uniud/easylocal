cmake_minimum_required(VERSION 3.22)
set(EASYLOCAL_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_PREFIX_PATH ${EASYLOCAL_PREFIX_PATH}/cmake)
include(${EASYLOCAL_PREFIX_PATH}/cmake/macos_modern_compiler.cmake)

project(EasyLocal CXX)

option(COMPILE_EXAMPLES "Compile also the EL basic examples" on)

if (COMPILE_EXAMPLES)
    add_subdirectory(examples)
endif()

