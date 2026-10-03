add_library(easylocal_pfsp_model INTERFACE)

target_link_libraries(
    easylocal_pfsp_model
    INTERFACE
        EasyLocal::Core
)

target_include_directories(
    easylocal_pfsp_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(
        easylocal_pfsp_tabu
        main.cpp
    )

    target_link_libraries(
        easylocal_pfsp_tabu
        PRIVATE
            easylocal_pfsp_model
    )

    target_compile_definitions(
        easylocal_pfsp_tabu
        PRIVATE
            EASYLOCAL_PFSP_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/medium.pfsp"
    )
endif()
