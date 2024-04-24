if (TARGET easylocal::easylocal)
    return()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ide.cmake)

get_filename_component(EASYLOCAL_SOURCE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

easylocal_extract_version(${EASYLOCAL_SOURCE_DIRECTORY})

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


file(GLOB headers ${EASYLOCAL_SOURCE_DIRECTORY}/include/easylocal/*.hh)

find_package(Boost 1.71 REQUIRED COMPONENTS program_options)

include(ExternalProject)

# Logging facilities

include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.12.0) # replace with latest revision

FetchContent_MakeAvailable(spdlog)

add_library(easylocal INTERFACE)
add_library(easylocal::easylocal ALIAS easylocal)
target_include_directories(easylocal INTERFACE $<BUILD_INTERFACE:${EASYLOCAL_SOURCE_DIRECTORY}/include/easylocal> $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/spdlog/include> $<INSTALL_INTERFACE:include/easylocal>)
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

if (NOT DEFINED easylocal_FIND_QUIETLY)
  message(STATUS "Found easylocal ${EASYLOCAL_VERSION} in ${EASYLOCAL_SOURCE_DIRECTORY}")
endif()
