if(NOT DEFINED CXX)
    message(FATAL_ERROR "CXX is required")
endif()
if(NOT DEFINED SOURCE)
    message(FATAL_ERROR "SOURCE is required")
endif()
if(NOT DEFINED INCLUDE_DIR)
    message(FATAL_ERROR "INCLUDE_DIR is required")
endif()
if(NOT DEFINED EXPECTED_DIAGNOSTIC)
    message(FATAL_ERROR "EXPECTED_DIAGNOSTIC is required")
endif()

# Syntax-only C++23 compilation, spelled for the compiler's command-line
# frontend: GNU (g++, clang++) or MSVC (cl, clang-cl).
if(FRONTEND STREQUAL "MSVC" AND CXX_ID STREQUAL "Clang")
    set(flags /clang:-std=c++23 /Zs /EHsc "/I${INCLUDE_DIR}")
elseif(FRONTEND STREQUAL "MSVC")
    set(flags /std:c++latest /permissive- /Zs /EHsc "/I${INCLUDE_DIR}")
else()
    set(flags -std=c++23 -fsyntax-only "-I${INCLUDE_DIR}")
    if(DEFINED SYSROOT AND NOT SYSROOT STREQUAL "")
        list(APPEND flags -isysroot "${SYSROOT}")
    endif()
endif()

# The build's own flags, such as -stdlib=libc++ or --gcc-install-dir=...
set(build_flags "")
if(DEFINED CXX_FLAGS AND NOT CXX_FLAGS STREQUAL "")
    separate_arguments(build_flags NATIVE_COMMAND "${CXX_FLAGS}")
endif()

execute_process(
    COMMAND
        "${CXX}"
        ${build_flags}
        ${flags}
        "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)

set(output "${stdout}\n${stderr}")

if(result EQUAL 0)
    message(FATAL_ERROR
        "Expected compilation to fail, but it succeeded for ${SOURCE}")
endif()

string(FIND "${output}" "${EXPECTED_DIAGNOSTIC}" diagnostic_position)
if(diagnostic_position EQUAL -1)
    message(FATAL_ERROR
        "Compilation failed for ${SOURCE}, but not with the expected diagnostic.\n"
        "Expected substring: ${EXPECTED_DIAGNOSTIC}\n"
        "Compiler output:\n${output}")
endif()

if(DEFINED EXPECTED_CONTEXT AND NOT EXPECTED_CONTEXT STREQUAL "")
    string(FIND "${output}" "${EXPECTED_CONTEXT}" context_position)
    if(context_position EQUAL -1)
        message(FATAL_ERROR
            "Compilation failed with the expected diagnostic, but did not expose "
            "the expected type/context information.\n"
            "Expected context substring: ${EXPECTED_CONTEXT}\n"
            "Compiler output:\n${output}")
    endif()
endif()
