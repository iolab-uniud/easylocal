add_executable(
    easylocal_neighborhood_traversal_benchmark
    traversal_benchmark.cpp
)
target_link_libraries(
    easylocal_neighborhood_traversal_benchmark
    PRIVATE
        easylocal_assignment_mwe_model
        easylocal_tsp_mwe_model
)

add_executable(
    easylocal_neighborhood_search_benchmark
    search_benchmark.cpp
)
target_link_libraries(
    easylocal_neighborhood_search_benchmark
    PRIVATE
        easylocal_assignment_mwe_model
        easylocal_tsp_mwe_model
)

add_executable(
    easylocal_trace_benchmark
    trace_benchmark.cpp
)
target_link_libraries(
    easylocal_trace_benchmark
    PRIVATE
        easylocal_assignment_mwe_model
)


add_executable(
    easylocal_trace_cost_encoding_benchmark
    trace_cost_encoding_benchmark.cpp
)
target_link_libraries(
    easylocal_trace_cost_encoding_benchmark
    PRIVATE
        EasyLocal::Core
)
