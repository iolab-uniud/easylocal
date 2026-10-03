cmake_minimum_required(VERSION 3.25)
project(tsp LANGUAGES CXX)

find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)

add_executable(tsp main.cpp)
target_link_libraries(tsp PRIVATE EasyLocal::Core)
