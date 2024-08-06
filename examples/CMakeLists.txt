cmake_minimum_required(VERSION 3.22)
project(poc-test CXX)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(easylocal_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

find_package(easylocal CONFIG REQUIRED)

# Add the main.cc file as an executable
add_executable(poc-test main.cc)
if (TARGET easylocal::single)
    target_link_libraries(poc-test PUBLIC easylocal::single)
    target_compile_definitions(poc-test PUBLIC EASYLOCAL_SINGLE)
else ()
    target_link_libraries(poc-test PUBLIC easylocal)
endif ()
#target_compile_features(poc-test PUBLIC cxx_std_23)
