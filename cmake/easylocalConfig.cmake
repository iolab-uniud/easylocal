if (TARGET easylocal::easylocal)
    return()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ide.cmake)

if (APPLE)
  # overrides AppleClang compiler with either llvm or gcc
  if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    include(${CMAKE_CURRENT_LIST_DIR}/macos_modern_compiler.cmake)
  endif()
endif ()

get_filename_component(EASYLOCAL_SOURCE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../src ABSOLUTE)

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

if (APPLE)
  file(GLOB BOOST_CELLAR_DIRS "${HOMEBREW_PREFIX}/Cellar/boost/*")
  foreach(BOOST_CELLAR_DIR ${BOOST_CELLAR_DIRS})
      if(IS_DIRECTORY ${BOOST_CELLAR_DIR})
          get_filename_component(BOOST_VERSION ${BOOST_CELLAR_DIR} NAME)
          message(STATUS "Found boost homebrew version: ${BOOST_VERSION}")
          set(Boost_ROOT ${BOOST_CELLAR_DIR})
          break()
      endif()
  endforeach()
endif()

find_package(Boost 1.85 REQUIRED COMPONENTS program_options)

include(ExternalProject)

# Logging facilities

message(STATUS "Downloading external projects")
include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.12.0) # replace with latest revision
FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
  FetchContent_MakeAvailable(spdlog)
endif()

add_library(easylocal INTERFACE)
add_library(easylocal::easylocal ALIAS easylocal)
target_include_directories(easylocal INTERFACE $<BUILD_INTERFACE:${EASYLOCAL_SOURCE_DIRECTORY}/include/easylocal> $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/spdlog/include> $<INSTALL_INTERFACE:include/easylocal>)
target_link_libraries(easylocal INTERFACE Boost::program_options spdlog::spdlog)
target_compile_features(easylocal INTERFACE cxx_std_23)
target_sources(easylocal INTERFACE ${headers})
set_property(TARGET easylocal PROPERTY CXX_STANDARD 23)
  
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14")
    message(FATAL_ERROR "Requires GCC 14 or later")
  endif()
  target_compile_options(easylocal INTERFACE $<BUILD_INTERFACE:-fcoroutines -Wall -Wextra -Wpedantic>)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.28")
    message(FATAL_ERROR "Requires MSVC 19.28 or later")    
  endif()
  target_compile_options(easylocal INTERFACE $<BUILD_INTERFACE:/W4>)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
  message(WARNING "Clang compiler has experimental coroutine support")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18")
    message(FATAL_ERROR "Requires Clang 18 or later")
  endif()
  target_compile_definitions(easylocal INTERFACE CLANG_COMPILER=1)
  target_compile_options(easylocal INTERFACE $<BUILD_INTERFACE:-Wall -Wextra -Wpedantic>)
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/check_object_from_this.cmake)

if (NOT DEFINED easylocal_FIND_QUIETLY)
  message(STATUS "Found easylocal ${EASYLOCAL_VERSION} in ${EASYLOCAL_SOURCE_DIRECTORY}")
endif()

message(STATUS "Experimental, including single header generation through Heady")

# Option to enable/disable the single header
option(SINGLE_HEADER "Enable single header generation" OFF)

if (SINGLE_HEADER)
  FetchContent_Declare(heady
    GIT_REPOSITORY https://github.com/JamesBoer/Heady.git
    GIT_TAG 876f730a30b4815ba6f657f222aaca23bfdc360f)

  FetchContent_GetProperties(Heady)
  if(NOT Heady_POPULATED)
    FetchContent_Populate(Heady)
  endif()

  message(STATUS "Heady source directory: ${heady_BINARY_DIR}")

  ExternalProject_Add(
    Heady
    SOURCE_DIR ${heady_SOURCE_DIR}
    BINARY_DIR ${heady_BINARY_DIR}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE:STRING=Release # Adjust build type as needed
    STEP_TARGETS build
    EXCLUDE_FROM_ALL TRUE
  )

  add_custom_command(
    OUTPUT ${EASYLOCAL_SOURCE_DIRECTORY}/../include/easylocal.hh
    COMMAND ${heady_BINARY_DIR}/Heady -r -s ${EASYLOCAL_SOURCE_DIRECTORY}/../src/include/easylocal -o ${EASYLOCAL_SOURCE_DIRECTORY}/../include/easylocal.hh
    DEPENDS ${headers} ${heady_BINARY_DIR}/Heady
  )

  # add_custom_target(single ALL DEPENDS ${EASYLOCAL_SOURCE_DIRECTORY}/../include/easylocal.hh)
  add_library(easylocal_single_header INTERFACE)
  target_include_directories(easylocal_single_header INTERFACE ${EASYLOCAL_SOURCE_DIRECTORY}/../include $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/spdlog/include>)
  target_link_libraries(easylocal_single_header INTERFACE Boost::program_options spdlog::spdlog)
  target_compile_features(easylocal_single_header INTERFACE cxx_std_23)
  target_sources(easylocal_single_header INTERFACE ${EASYLOCAL_SOURCE_DIRECTORY}/../include/easylocal.hh)
  set_property(TARGET easylocal_single_header PROPERTY CXX_STANDARD 23)

  add_library(easylocal::single ALIAS easylocal_single_header)

  endif (SINGLE_HEADER)
