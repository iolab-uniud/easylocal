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

# Component layering: a header may include only headers of its own or of a
# lower layer.
#   0 utils, config, trace   1 cost   2 helpers   3 runners, testing
#   4 solvers         5 app    6 adapters
# Root umbrella headers take the layer of the directory they aggregate.
function(easylocal_header_layer relative out_var)
    if(relative MATCHES "^(utils|config|trace)(/|\\.hpp$)")
        set(_layer 0)
    elseif(relative MATCHES "^cost(/|\\.hpp$)")
        set(_layer 1)
    elseif(relative MATCHES "^helpers(/|\\.hpp$)")
        set(_layer 2)
    elseif(relative MATCHES "^(runners|testing)(/|\\.hpp$)")
        set(_layer 3)
    elseif(relative MATCHES "^solvers(/|\\.hpp$)")
        set(_layer 4)
    elseif(relative MATCHES "^app(/|\\.hpp$)" OR relative STREQUAL "easylocal.hpp")
        set(_layer 5)
    elseif(relative MATCHES "^adapters(/|\\.hpp$)")
        set(_layer 6)
    else()
        message(FATAL_ERROR "Header outside the component layout: easylocal/${relative}")
    endif()
    set(${out_var} ${_layer} PARENT_SCOPE)
endfunction()

foreach(_header IN LISTS _all_headers)
    file(RELATIVE_PATH _relative "${_include_root}" "${_header}")
    easylocal_header_layer("${_relative}" _own_layer)

    file(STRINGS "${_header}" _includes REGEX "^[ \t]*#[ \t]*include[ \t]*<easylocal/")
    foreach(_include IN LISTS _includes)
        string(REGEX REPLACE "^[^<]*<easylocal/([^>]+)>.*$" "\\1" _target "${_include}")
        if(_target MATCHES "^third_party/")
            continue()
        endif()
        easylocal_header_layer("${_target}" _target_layer)
        if(_target_layer GREATER _own_layer)
            message(FATAL_ERROR
                "Layering violation: easylocal/${_relative} (layer ${_own_layer}) "
                "includes easylocal/${_target} (layer ${_target_layer})")
        endif()
    endforeach()
endforeach()

message(STATUS "EasyLocal Core/adapter header boundary and layering are clean")
