add_library(easylocal_tsp_mwe_model INTERFACE)

target_link_libraries(
    easylocal_tsp_mwe_model
    INTERFACE
        EasyLocal::Core
)

target_include_directories(
    easylocal_tsp_mwe_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(
        easylocal_tsp_sa_mwe
        sa_main.cpp
    )

    target_link_libraries(
        easylocal_tsp_sa_mwe
        PRIVATE
            easylocal_tsp_mwe_model
    )

    target_compile_definitions(
        easylocal_tsp_sa_mwe
        PRIVATE
            EASYLOCAL_TSP_MWE_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.tsp"
    )
endif()
