add_library(easylocal_exam_timetabling_mwe_model INTERFACE)

target_link_libraries(
    easylocal_exam_timetabling_mwe_model
    INTERFACE
        EasyLocal::EasyLocal
)

target_include_directories(
    easylocal_exam_timetabling_mwe_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EASYLOCAL_BUILD_EXAMPLES)
    add_executable(
        easylocal_exam_timetabling_mwe
        main.cpp
    )

    target_link_libraries(
        easylocal_exam_timetabling_mwe
        PRIVATE
            easylocal_exam_timetabling_mwe_model
    )

    target_compile_definitions(
        easylocal_exam_timetabling_mwe
        PRIVATE
            EASYLOCAL_EXAM_MWE_INSTANCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/instances/small.exam"
    )
endif()
