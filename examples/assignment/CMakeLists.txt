add_library(easylocal_assignment_model INTERFACE)

target_link_libraries(
    easylocal_assignment_model
    INTERFACE
        EasyLocal::Core
)

target_include_directories(
    easylocal_assignment_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(
        easylocal_assignment
        main.cpp
    )

    target_link_libraries(
        easylocal_assignment
        PRIVATE
            easylocal_assignment_model
    )

    target_compile_definitions(
        easylocal_assignment
        PRIVATE
            EASYLOCAL_ASSIGNMENT_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.assignment"
    )
endif()

if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::TUI)
    add_executable(
        easylocal_assignment_tui
        tui_main.cpp
    )

    target_link_libraries(
        easylocal_assignment_tui
        PRIVATE
            easylocal_assignment_model
            EasyLocal::TUI
    )

    target_compile_definitions(
        easylocal_assignment_tui
        PRIVATE
            EASYLOCAL_ASSIGNMENT_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/large.assignment"
    )
endif()


if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::REST)
    add_executable(
        easylocal_assignment_rest
        rest_main.cpp
    )

    target_link_libraries(
        easylocal_assignment_rest
        PRIVATE
            easylocal_assignment_model
            EasyLocal::REST
    )
endif()
