cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EASYLOCAL_SOURCE_DIR OR EASYLOCAL_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "EASYLOCAL_SOURCE_DIR must be provided")
endif()

set(_include_root "${EASYLOCAL_SOURCE_DIR}/include/easylocal")
file(GLOB_RECURSE _all_headers "${_include_root}/*.hpp")

set(_core_headers)
set(_adapter_headers)
foreach(_header IN LISTS _all_headers)
    if(_header MATCHES "/easylocal/adapters/")
        list(APPEND _adapter_headers "${_header}")
    else()
        list(APPEND _core_headers "${_header}")
    endif()
endforeach()

foreach(_header IN LISTS _core_headers)
    file(READ "${_header}" _contents)

    if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]easylocal/adapters/")
        message(FATAL_ERROR
            "Core header depends on an optional adapter: ${_header}")
    endif()
    if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]ftxui/")
        message(FATAL_ERROR
            "Core header depends directly on FTXUI: ${_header}")
    endif()
    if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]toml\\+\\+/")
        message(FATAL_ERROR
            "Core header depends directly on toml++: ${_header}")
    endif()
    if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]crow")
        message(FATAL_ERROR
            "Core header depends directly on Crow: ${_header}")
    endif()
endforeach()

foreach(_header IN LISTS _adapter_headers)
    file(READ "${_header}" _contents)

    if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]easylocal/([a-z_]+/)*detail/")
        message(FATAL_ERROR
            "Optional adapter reaches into EasyLocal detail headers: ${_header}")
    endif()
endforeach()

message(STATUS "EasyLocal Core/adapter header boundary is clean")
