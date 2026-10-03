if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(easylocal_tutorial main.cpp)
    target_link_libraries(easylocal_tutorial PRIVATE EasyLocal::Core)

    add_executable(easylocal_tutorial_checks checks.cpp)
    target_link_libraries(easylocal_tutorial_checks PRIVATE EasyLocal::Core)
endif()

if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::TUI)
    add_executable(easylocal_tutorial_tui tui_main.cpp)
    target_link_libraries(easylocal_tutorial_tui PRIVATE EasyLocal::TUI)
    target_compile_definitions(
        easylocal_tutorial_tui
        PRIVATE EASYLOCAL_TUTORIAL_INSTANCE="${CMAKE_CURRENT_SOURCE_DIR}/five.tsp")
endif()

if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::REST)
    add_executable(easylocal_tutorial_rest rest_main.cpp)
    target_link_libraries(easylocal_tutorial_rest PRIVATE EasyLocal::REST)
endif()

if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::ConfigTOML)
    add_executable(easylocal_tutorial_toml toml_main.cpp)
    target_link_libraries(easylocal_tutorial_toml PRIVATE EasyLocal::Core EasyLocal::ConfigTOML)
    target_compile_definitions(
        easylocal_tutorial_toml
        PRIVATE EASYLOCAL_TUTORIAL_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/annealing.toml")
endif()
