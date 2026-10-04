add_library(easylocal_exam_timetabling_model INTERFACE)

target_link_libraries(
    easylocal_exam_timetabling_model
    INTERFACE
        EasyLocal::Core
)

target_include_directories(
    easylocal_exam_timetabling_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(
        easylocal_exam_timetabling
        main.cpp
    )

    target_link_libraries(
        easylocal_exam_timetabling
        PRIVATE
            easylocal_exam_timetabling_model
    )

    target_compile_definitions(
        easylocal_exam_timetabling
        PRIVATE
            EASYLOCAL_EXAM_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.exam"
    )
endif()
