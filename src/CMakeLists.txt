# todo complete the set of headers
file(GLOB hdrs "${CMAKE_CURRENT_SOURCE_DIR}/*.hh ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.hh ${CMAKE_CURRENT_SOURCE_DIR}/runners/*.hh")
add_library(EasyLocalpp utils/Random.cc utils/Types.cc utils/Parameter.cc ${hdrs})
target_link_libraries(EasyLocalpp ${Boost_LIBRARIES})

source_group("Header Files" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/([.]|helpers|runners|solvers)/[^/]*([.]h|[.]hpp|[.]hh)")
