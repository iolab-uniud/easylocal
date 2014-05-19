cmake_minimum_required(VERSION 2.8)

project(EasyLocalpp C CXX)

find_package(Threads)

set(CMAKE_CXX_FLAGS "-std=c++11")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${EasyLocalpp_SOURCE_DIR}/cmake)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

find_package(Boost 1.45.0 COMPONENTS program_options) 

include_directories(${EasyLocalpp_SOURCE_DIR}/src ${EasyLocalpp_SOURCE_DIR}/test ${Boost_INCLUDE_DIR})
link_directories(${EasyLocalpp_SOURCE_DIR}/src)

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen.cfg ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )
endif(DOXYGEN_FOUND)

add_subdirectory(src)
#add_subdirectory(test)

