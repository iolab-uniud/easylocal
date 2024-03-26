cmake_minimum_required(VERSION 3.22)

include(cmake/utils.cmake)
include(cmake/ide.cmake)

easylocal_extract_version()

project(easylocal VERSION ${EASYLOCAL_VERSION} LANGUAGES CXX)
# ---------------------------------------------------------------------------------------
# Compiler config
# ---------------------------------------------------------------------------------------
# c++ standard >=23 is required
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
elseif(CMAKE_CXX_STANDARD LESS 23)
    message(FATAL_ERROR "Minimum supported CMAKE_CXX_STANDARD is 23, but it is set to ${CMAKE_CXX_STANDARD}")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------------------
# Set default build to release
# ---------------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()


file(GLOB headers include/*.hh)

find_package(Boost 1.71 REQUIRED COMPONENTS program_options)

include(ExternalProject)

# Logging facilities
# ExternalProject_Add(spdlog 
#     GIT_REPOSITORY https://github.com/gabime/spdlog
#     GIT_TAG 7e635fca68d014934b4af8a1cf874f63989352b7 # replace with latest revision
#     SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/spdlog"
# )
#add_subdirectory(3rdparty/spdlog)

include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.12.0) # replace with latest revision

FetchContent_MakeAvailable(spdlog)

add_library(easylocal INTERFACE)
add_library(easylocal::easylocal ALIAS easylocal)
target_include_directories(easylocal INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/spdlog/include> $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_link_libraries(easylocal INTERFACE Boost::program_options spdlog::spdlog)
target_compile_features(easylocal INTERFACE cxx_std_23)
target_sources(easylocal INTERFACE ${headers})
set_property(TARGET easylocal PROPERTY CXX_STANDARD 23)
  
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10")
    message(WARNING "Requires GCC 10 or later")
  endif()
  target_compile_options(easylocal INTERFACE $<BUILD_INTERFACE:-fcoroutines -Wall -Wextra -Wpedantic>)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.28")
    message(WARNING "Requires MSVC 19.28 or later")    
  endif()
  target_compile_options(easylocal INTERFACE $<BUILD_INTERFACE:/W4>)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
  message(WARNING "Clang compiler has experimental coroutine support")
  target_compile_definitions(easylocal INTERFACE CLANG_COMPILER=1)
  target_compile_options(easylocal INTERFACE $<BUILD_INTERFACE:-Wall -Wextra -Wpedantic>)
endif ()

add_subdirectory(examples)