cmake_minimum_required(VERSION 3.25)

# Runs COMMAND (a list) and checks its exit status and its output (stdout and
# stderr together): the status must be EXIT_CODE, and the output must match
# the regular expression OUTPUT, where \n stands for a newline. A
# PASS_REGULAR_EXPRESSION alone accepts any exit status, a crash included, and
# WILL_FAIL any failure.

foreach(_required IN ITEMS COMMAND EXIT_CODE OUTPUT)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} must be provided")
    endif()
endforeach()

execute_process(
    COMMAND ${COMMAND}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _output
)

string(REPLACE "\\n" "\n" _expected "${OUTPUT}")
if(NOT "${_result}" STREQUAL "${EXIT_CODE}")
    message(FATAL_ERROR
        "exit status ${_result}, expected ${EXIT_CODE}; the output:\n${_output}")
endif()
if(NOT _output MATCHES "${_expected}")
    message(FATAL_ERROR
        "the output does not match '${OUTPUT}':\n${_output}")
endif()
message(STATUS "exit status ${_result}; the output:\n${_output}")
