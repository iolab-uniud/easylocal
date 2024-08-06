cmake_minimum_required(VERSION 3.22)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_modern_compiler.cmake)

project(EasyLocal CXX)

add_subdirectory(examples)

