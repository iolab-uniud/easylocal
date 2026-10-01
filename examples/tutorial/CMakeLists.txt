if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(easylocal_tutorial main.cpp)
    target_link_libraries(easylocal_tutorial PRIVATE EasyLocal::Core)

    add_executable(easylocal_tutorial_checks checks.cpp)
    target_link_libraries(easylocal_tutorial_checks PRIVATE EasyLocal::Core)
endif()
