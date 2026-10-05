add_library(easylocal_tsp_model INTERFACE)

target_link_libraries(
    easylocal_tsp_model
    INTERFACE
        EasyLocal::Core
)

target_include_directories(
    easylocal_tsp_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(
        easylocal_tsp_sa
        sa_main.cpp
    )

    target_link_libraries(
        easylocal_tsp_sa
        PRIVATE
            easylocal_tsp_model
    )

    target_compile_definitions(
        easylocal_tsp_sa
        PRIVATE
            EASYLOCAL_TSP_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.tsp"
    )

    add_executable(
        easylocal_tsp_two_neighborhoods
        two_neighborhoods.cpp
    )

    target_link_libraries(
        easylocal_tsp_two_neighborhoods
        PRIVATE
            easylocal_tsp_model
    )

    target_compile_definitions(
        easylocal_tsp_two_neighborhoods
        PRIVATE
            EASYLOCAL_TSP_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.tsp"
    )
endif()


if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::TUI)
    add_executable(
        easylocal_tsp_tui
        tui_main.cpp
    )

    target_link_libraries(
        easylocal_tsp_tui
        PRIVATE
            easylocal_tsp_model
            EasyLocal::TUI
    )

    # The tester saves its solution (W) over a copy in the build directory,
    # not over the checked-in one.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/instances/small.sol"
        "${CMAKE_CURRENT_BINARY_DIR}/small.sol"
        COPYONLY)
    target_compile_definitions(
        easylocal_tsp_tui
        PRIVATE
            EASYLOCAL_TSP_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.tsp"
            EASYLOCAL_TSP_SOLUTION_FILE="${CMAKE_CURRENT_BINARY_DIR}/small.sol"
    )
endif()
