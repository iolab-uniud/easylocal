cmake_minimum_required(VERSION 3.22)
project(poc-test CXX)

#set(easylocal_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

find_package(easylocal CONFIG REQUIRED)

# Add the main.cc file as an executable
add_executable(poc-test main.cc)
target_link_libraries(poc-test PUBLIC easylocal::easylocal)
#target_compile_features(poc-test PUBLIC cxx_std_23)
