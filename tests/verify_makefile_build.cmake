cmake_minimum_required(VERSION 3.25)

# The build without CMake of docs/tutorial/17-building.md: the quick start
# compiled with one command, and the tutorial's makefile building the programs
# of Core and of every optional component this build enables. The headers come
# from the installation the producing build was installed to, as a student's
# prefix would hold them.

foreach(_required IN ITEMS
        EASYLOCAL_BUILD_DIR
        EASYLOCAL_CXX_COMPILER
        EASYLOCAL_EXAMPLE_DIR
        EASYLOCAL_MAKE
        EASYLOCAL_QUICKSTART_DIR
        EASYLOCAL_WORK_DIR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} must be provided")
    endif()
endforeach()

# Runs the command held in the list variable command_variable: passed by name,
# so that an element with spaces (a makefile variable of several flags) stays
# one argument.
function(easylocal_run_checked description command_variable)
    execute_process(
        COMMAND ${${command_variable}}
        RESULT_VARIABLE _result
        COMMAND_ECHO STDOUT
        ${ARGN}
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "${description} failed with exit code ${_result}")
    endif()
endfunction()

# "-isystem <dir>" for each directory of the list, as the makefile writes the
# flags of a component; a dependency without include directories leaves an
# empty entry, which goes.
function(easylocal_include_flags out directories)
    set(_directories "${directories}")
    list(REMOVE_ITEM _directories "")
    list(REMOVE_DUPLICATES _directories)
    set(_flags "")
    foreach(_directory IN LISTS _directories)
        list(APPEND _flags "-isystem ${_directory}")
    endforeach()
    list(JOIN _flags " " _joined)
    set(${out} "${_joined}" PARENT_SCOPE)
endfunction()

# "-D<definition>" for each compile definition of a dependency.
function(easylocal_definition_flags out definitions)
    set(_definitions "${definitions}")
    list(REMOVE_ITEM _definitions "")
    list(REMOVE_DUPLICATES _definitions)
    set(_flags "")
    foreach(_definition IN LISTS _definitions)
        list(APPEND _flags "-D${_definition}")
    endforeach()
    list(JOIN _flags " " _joined)
    set(${out} "${_joined}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${EASYLOCAL_WORK_DIR}")

# The headers as an installation provides them.
set(_prefix "${EASYLOCAL_WORK_DIR}/prefix")
set(_install_command
    "${CMAKE_COMMAND}" --install "${EASYLOCAL_BUILD_DIR}" --prefix "${_prefix}")
if(DEFINED EASYLOCAL_CONFIG AND NOT "${EASYLOCAL_CONFIG}" STREQUAL "")
    list(APPEND _install_command --config "${EASYLOCAL_CONFIG}")
endif()
easylocal_run_checked("installing EasyLocal" _install_command)

separate_arguments(_extra_flags NATIVE_COMMAND "${EASYLOCAL_CXX_FLAGS}")
set(_cxxflags -std=c++23 -O2 -Wall -Wextra ${_extra_flags})
list(JOIN _cxxflags " " _cxxflags_value)

# The quick start, built with one command as the chapter opens.
set(_quickstart "${EASYLOCAL_WORK_DIR}/quickstart")
file(COPY "${EASYLOCAL_QUICKSTART_DIR}/" DESTINATION "${_quickstart}")
set(_compile_command
    "${EASYLOCAL_CXX_COMPILER}"
    ${_cxxflags}
    "-I${_prefix}/include"
    "${_quickstart}/main.cpp"
    -o "${_quickstart}/tsp"
)
easylocal_run_checked("compiling the quick start" _compile_command)
set(_run_command "${_quickstart}/tsp")
easylocal_run_checked("running the quick start" _run_command
    WORKING_DIRECTORY "${_quickstart}")

# The tutorial's makefile, in a copy of the example directory so that the
# objects and the programs stay out of the source tree.
set(_tutorial "${EASYLOCAL_WORK_DIR}/tutorial")
file(COPY "${EASYLOCAL_EXAMPLE_DIR}/" DESTINATION "${_tutorial}")
if(NOT EXISTS "${_tutorial}/Makefile")
    message(FATAL_ERROR "the tutorial example has no Makefile")
endif()

set(_make_arguments
    "${EASYLOCAL_MAKE}"
    -C "${_tutorial}"
    "EASYLOCAL=${_prefix}"
    "CXX=${EASYLOCAL_CXX_COMPILER}"
    "CXXFLAGS=${_cxxflags_value}"
)

# The optional components: their flags come from the libraries this build
# found or fetched, in place of the prefixes the makefile derives them from.
set(_targets all)

if(EASYLOCAL_TUI_ENABLED)
    easylocal_include_flags(_tui_includes "${EASYLOCAL_TUI_INCLUDE_DIRS}")
    easylocal_definition_flags(_tui_definitions "${EASYLOCAL_TUI_DEFINITIONS}")
    list(JOIN EASYLOCAL_TUI_LINK_LIBRARIES " " _tui_libraries)
    list(APPEND _make_arguments
        "TUI_CPPFLAGS=${_tui_includes} ${_tui_definitions}"
        "TUI_LDLIBS=${_tui_libraries} ${EASYLOCAL_THREAD_FLAG}")
    list(APPEND _targets tsp_tui)
endif()

if(EASYLOCAL_REST_ENABLED)
    easylocal_include_flags(_rest_includes "${EASYLOCAL_REST_INCLUDE_DIRS}")
    easylocal_definition_flags(_rest_definitions "${EASYLOCAL_REST_DEFINITIONS}")
    list(APPEND _make_arguments
        "REST_CPPFLAGS=${_rest_includes} ${_rest_definitions}"
        "REST_LDLIBS=${EASYLOCAL_THREAD_FLAG}")
    list(APPEND _targets tsp_rest)
endif()

if(EASYLOCAL_CONFIG_TOML_ENABLED)
    easylocal_include_flags(_toml_includes "${EASYLOCAL_CONFIG_TOML_INCLUDE_DIRS}")
    easylocal_definition_flags(_toml_definitions "${EASYLOCAL_CONFIG_TOML_DEFINITIONS}")
    list(APPEND _make_arguments
        "TOML_CPPFLAGS=${_toml_includes} ${_toml_definitions}")
    list(APPEND _targets tsp_toml)
endif()

set(_make_command ${_make_arguments} ${_targets})
easylocal_run_checked("building the tutorial with its makefile" _make_command)

# The programs that run without a terminal or a port: the search, the command
# line, the component checks and, with ConfigTOML, the configured run.
set(_tsp_command "${_tutorial}/tsp")
easylocal_run_checked("running tsp" _tsp_command WORKING_DIRECTORY "${_tutorial}")

set(_cli_command "${_tutorial}/tsp_cli" --help)
easylocal_run_checked("running tsp_cli --help" _cli_command
    WORKING_DIRECTORY "${_tutorial}")

set(_checks_command "${_tutorial}/tsp_checks")
easylocal_run_checked("running tsp_checks" _checks_command
    WORKING_DIRECTORY "${_tutorial}")

if(EASYLOCAL_CONFIG_TOML_ENABLED)
    set(_toml_command "${_tutorial}/tsp_toml" annealing.toml)
    easylocal_run_checked("running tsp_toml" _toml_command
        WORKING_DIRECTORY "${_tutorial}")
endif()

# The makefile cleans up after itself: the programs and the objects go.
set(_clean_command "${EASYLOCAL_MAKE}" -C "${_tutorial}" clean)
easylocal_run_checked("cleaning the tutorial build" _clean_command)
file(GLOB _leftovers "${_tutorial}/*.o" "${_tutorial}/tsp" "${_tutorial}/tsp_*")
if(_leftovers)
    message(FATAL_ERROR "make clean left files behind: ${_leftovers}")
endif()
