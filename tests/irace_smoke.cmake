cmake_minimum_required(VERSION 3.25)

# A smoke test of irace on a program built with cli::run: --tuning.irace
# writes the scenario, irace checks it and runs a short tuning. Skipped when
# R or its irace package are not installed.

foreach(_required IN ITEMS EASYLOCAL_PROGRAM EASYLOCAL_WORK_DIR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} must be provided")
    endif()
endforeach()

# EASYLOCAL_RSCRIPT may name Rscript; empty, or not found, skips the test.
if(NOT DEFINED EASYLOCAL_RSCRIPT)
    find_program(EASYLOCAL_RSCRIPT Rscript HINTS /opt/homebrew/bin /usr/local/bin)
endif()
set(_rscript "${EASYLOCAL_RSCRIPT}")
if(NOT _rscript)
    message("SKIPPED: irace is not available (no Rscript)")
    return()
endif()
execute_process(
    COMMAND "${_rscript}" -e "cat(system.file(package = 'irace'))"
    OUTPUT_VARIABLE _irace_dir
    RESULT_VARIABLE _result
    ERROR_QUIET
)
if(NOT _result EQUAL 0 OR "${_irace_dir}" STREQUAL "")
    message("SKIPPED: irace is not available (no irace package in R)")
    return()
endif()
set(_irace "${_irace_dir}/bin/irace")
cmake_path(GET _rscript PARENT_PATH _r_bin)
set(ENV{PATH} "${_r_bin}:$ENV{PATH}")

file(REMOVE_RECURSE "${EASYLOCAL_WORK_DIR}")
execute_process(
    COMMAND "${EASYLOCAL_PROGRAM}" "--tuning.irace=${EASYLOCAL_WORK_DIR}"
        --runners.sa.temperature.allowed_iterations=2000
    RESULT_VARIABLE _result
)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "--tuning.irace failed with exit code ${_result}")
endif()

# The smallest budget irace accepts for this scenario is about 100 runs.
file(READ "${EASYLOCAL_WORK_DIR}/scenario.txt" _scenario)
string(REPLACE "maxExperiments = 1000" "maxExperiments = 120" _scenario "${_scenario}")
file(WRITE "${EASYLOCAL_WORK_DIR}/scenario.txt" "${_scenario}")

foreach(_mode IN ITEMS --check "")
    execute_process(
        COMMAND "${_irace}" ${_mode}
        WORKING_DIRECTORY "${EASYLOCAL_WORK_DIR}"
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _output
        RESULT_VARIABLE _result
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "irace ${_mode} failed with exit code ${_result}:\n${_output}")
    endif()
endforeach()
if(NOT _output MATCHES "Best-so-far configuration")
    message(FATAL_ERROR "irace found no configuration:\n${_output}")
endif()
string(REGEX MATCH "# Best configurations as commandlines[^\n]*\n[^\n]*" _best "${_output}")
message("${_best}")
