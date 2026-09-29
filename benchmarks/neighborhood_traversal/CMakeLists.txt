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

# spdlog is benchmark-only and is never fetched by EasyLocal.  Local builds may
# omit it; CI/release benchmark jobs set EASYLOCAL_BENCHMARK_REQUIRE_SPDLOG=ON.
find_package(spdlog CONFIG QUIET)
if(spdlog_FOUND)
    target_link_libraries(
        easylocal_trace_benchmark
        PRIVATE
            spdlog::spdlog
    )
    target_compile_definitions(
        easylocal_trace_benchmark
        PRIVATE
            EASYLOCAL_TRACE_BENCHMARK_HAS_SPDLOG=1
    )
elseif(EASYLOCAL_BENCHMARK_REQUIRE_SPDLOG)
    message(FATAL_ERROR
        "Trace benchmark comparison requires a system spdlog package. "
        "Install spdlog or configure with "
        "EASYLOCAL_BENCHMARK_REQUIRE_SPDLOG=OFF.")
else()
    message(STATUS
        "spdlog not found: trace benchmark will omit the spdlog comparison")
endif()
