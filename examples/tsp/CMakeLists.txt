add_library(easylocal_tsp_mwe_model INTERFACE)

target_link_libraries(
    easylocal_tsp_mwe_model
    INTERFACE
        EasyLocal::EasyLocal
)

target_include_directories(
    easylocal_tsp_mwe_model
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
