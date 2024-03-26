cmake_minimum_required(VERSION 3.22)
project(poc-test CXX)

if(NOT TARGET easylocal::easylocal)
    # Stand-alone build
    find_package(easylocal REQUIRED)
    add_library(easylocal::easylocal IMPORTED INTERFACE)
endif()

# Add the main.cc file as an executable
add_executable(poc-test main.cc)
target_link_libraries(poc-test PUBLIC easylocal::easylocal)
#target_compile_features(poc-test PUBLIC cxx_std_23)
