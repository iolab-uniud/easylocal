find_package(Threads)
find_package(Boost 1.45.0 COMPONENTS program_options) 

include_directories(${Boost_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(helpers)
add_subdirectory(utils)
add_subdirectory(kickers)
add_subdirectory(observers)
add_subdirectory(runners)
add_subdirectory(solvers)
add_subdirectory(testers)
add_subdirectory(unit)

add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:Utils>)
target_link_libraries(EasyLocalpp ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})