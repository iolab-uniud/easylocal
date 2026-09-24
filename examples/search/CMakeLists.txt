add_library(easylocal_mwe_search INTERFACE)

target_include_directories(
    easylocal_mwe_search
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
