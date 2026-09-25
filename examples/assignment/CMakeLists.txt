add_library(easylocal_assignment_mwe_model INTERFACE)

target_link_libraries(
    easylocal_assignment_mwe_model
    INTERFACE
        EasyLocal::Core
)

target_include_directories(
    easylocal_assignment_mwe_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(
        easylocal_assignment_mwe
        main.cpp
    )

    target_link_libraries(
        easylocal_assignment_mwe
        PRIVATE
            easylocal_assignment_mwe_model
    )

    target_compile_definitions(
        easylocal_assignment_mwe
        PRIVATE
            EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.assignment"
    )
endif()
