if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(easylocal_quickstart main.cpp)
    target_link_libraries(easylocal_quickstart PRIVATE EasyLocal::Core)
endif()
