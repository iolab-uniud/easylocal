if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(easylocal_tutorial main.cpp)
    target_link_libraries(easylocal_tutorial PRIVATE EasyLocal::Core)

    add_executable(easylocal_tutorial_checks checks.cpp)
    target_link_libraries(easylocal_tutorial_checks PRIVATE EasyLocal::Core)
endif()

if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::TUI)
    add_executable(easylocal_tutorial_tui tui_main.cpp)
    target_link_libraries(easylocal_tutorial_tui PRIVATE EasyLocal::TUI)
endif()

if(EASYLOCAL_BUILD_EXAMPLES AND TARGET EasyLocal::REST)
    add_executable(easylocal_tutorial_rest rest_main.cpp)
    target_link_libraries(easylocal_tutorial_rest PRIVATE EasyLocal::REST)
endif()
