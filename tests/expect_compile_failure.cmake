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

execute_process(
    COMMAND
        "${CXX}"
        -std=c++23
        -fsyntax-only
        "-I${INCLUDE_DIR}"
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
