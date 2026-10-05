easylocal_configure_own_targets()

# The tests check with assert(): keep it active in every configuration, so a
# Release build still verifies something.
if(MSVC)
    add_compile_options(/UNDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()

# Compile every installed header in its own translation unit.  Keeping the
# probes generated from the include tree makes self-containment coverage follow
# the public surface automatically instead of relying on one hand-written test
# source per header.
function(easylocal_add_header_self_containment_test name dependency)
    set(headers ${ARGN})
    set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/header-self-containment/${name}")
    file(MAKE_DIRECTORY "${probe_dir}")

    set(sources)
    foreach(header IN LISTS headers)
        string(MAKE_C_IDENTIFIER "${header}" probe_id)
        set(source "${probe_dir}/${probe_id}.cpp")
        file(WRITE "${source}"
            "#include <${header}>\n\n"
            "void easylocal_header_probe_${probe_id}() {}\n")
        list(APPEND sources "${source}")
    endforeach()

    set(main_source "${probe_dir}/main.cpp")
    file(WRITE "${main_source}" "int main() { return 0; }\n")
    list(APPEND sources "${main_source}")

    set(target "easylocal_${name}_headers_self_containment")
    add_executable("${target}" ${sources})
    target_link_libraries("${target}" PRIVATE "${dependency}")
    add_test(
        NAME "easylocal.${name}-headers-self-containment"
        COMMAND "${target}"
    )
endfunction()

# A test program: the executable easylocal_<name>_test (dashes as underscores),
# built from SOURCES and linked with LIBRARIES, run as the test easylocal.<name>.
function(easylocal_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES;DEFINITIONS")
    string(REPLACE "-" "_" target "easylocal_${name}_test")
    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE ${arg_LIBRARIES})
    if(arg_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${arg_DEFINITIONS})
    endif()
    add_test(NAME easylocal.${name} COMMAND ${target})
endfunction()

file(
    GLOB_RECURSE EASYLOCAL_CORE_HEADERS
    CONFIGURE_DEPENDS
    RELATIVE "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/easylocal/*.hpp"
)
list(FILTER EASYLOCAL_CORE_HEADERS EXCLUDE REGEX "^easylocal/adapters/")

# Headers under detail/ are installed because public templates include them, but
# they are implementation headers rather than supported direct-entry API.  Keep
# their self-containment checked without counting them as the public surface.
set(EASYLOCAL_CORE_PUBLIC_HEADERS ${EASYLOCAL_CORE_HEADERS})
list(FILTER EASYLOCAL_CORE_PUBLIC_HEADERS EXCLUDE REGEX "(^|/)detail/")

set(EASYLOCAL_CORE_DETAIL_HEADERS ${EASYLOCAL_CORE_HEADERS})
list(FILTER EASYLOCAL_CORE_DETAIL_HEADERS INCLUDE REGEX "(^|/)detail/")

easylocal_add_header_self_containment_test(
    core-public
    EasyLocal::Core
    ${EASYLOCAL_CORE_PUBLIC_HEADERS}
)

easylocal_add_header_self_containment_test(
    core-detail
    EasyLocal::Core
    ${EASYLOCAL_CORE_DETAIL_HEADERS}
)

if(TARGET EasyLocal::ConfigTOML)
    easylocal_add_header_self_containment_test(
        config-toml
        EasyLocal::ConfigTOML
        easylocal/adapters/toml.hpp
    )
endif()

if(TARGET EasyLocal::TUI)
    file(
        GLOB EASYLOCAL_TUI_HEADERS
        CONFIGURE_DEPENDS
        RELATIVE "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include/easylocal/adapters/tui/*.hpp"
    )
    list(APPEND EASYLOCAL_TUI_HEADERS easylocal/adapters/tui.hpp)
    easylocal_add_header_self_containment_test(
        tui
        EasyLocal::TUI
        ${EASYLOCAL_TUI_HEADERS}
    )
endif()

if(TARGET EasyLocal::REST)
    file(
        GLOB EASYLOCAL_REST_HEADERS
        CONFIGURE_DEPENDS
        RELATIVE "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include/easylocal/adapters/rest/*.hpp"
    )
    list(APPEND EASYLOCAL_REST_HEADERS easylocal/adapters/rest.hpp)
    easylocal_add_header_self_containment_test(
        rest
        EasyLocal::REST
        ${EASYLOCAL_REST_HEADERS}
    )
endif()

add_test(
    NAME easylocal.architecture-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_architecture_boundary.cmake"
)

# No raw [[no_unique_address]], which MSVC and clang-cl ignore; the check is
# shown to catch one on a generated file.
add_test(
    NAME easylocal.portable-attributes
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_portable_attributes.cmake"
)
set(easylocal_raw_attribute_dir "${CMAKE_CURRENT_BINARY_DIR}/raw_attribute")
file(WRITE "${easylocal_raw_attribute_dir}/raw.hpp"
    "struct empty {};\nstruct holder { [[ no_unique_address ]] empty member; };\n")
add_test(
    NAME easylocal.portable-attributes-catches-raw
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_SCAN_DIRS=${easylocal_raw_attribute_dir}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_portable_attributes.cmake"
)
set_tests_properties(
    easylocal.portable-attributes-catches-raw
    PROPERTIES PASS_REGULAR_EXPRESSION "raw\\.hpp:2")

# No deduced std::tuple in the library, which some compilers find ambiguous
# with one argument; the check is shown to catch one on a generated file.
add_test(
    NAME easylocal.tuple-types
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_tuple_types.cmake"
)
set(easylocal_deduced_tuple_dir "${CMAKE_CURRENT_BINARY_DIR}/deduced_tuple")
file(WRITE "${easylocal_deduced_tuple_dir}/deduced.hpp"
    "#include <tuple>\ninline auto one = std::tuple{1};\n")
add_test(
    NAME easylocal.tuple-types-catches-deduced
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_SCAN_DIRS=${easylocal_deduced_tuple_dir}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_tuple_types.cmake"
)
set_tests_properties(
    easylocal.tuple-types-catches-deduced
    PROPERTIES PASS_REGULAR_EXPRESSION "deduced\\.hpp:2")



if(TARGET EasyLocal::ConfigTOML)
    easylocal_add_test(
        configuration-toml
        SOURCES
            configuration_toml.cpp
        LIBRARIES
            EasyLocal::ConfigTOML
    )
endif()

easylocal_add_test(
    factory-api
    SOURCES
        factory_api.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    logging
    SOURCES
        logging.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    run-control
    SOURCES
        run_control.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    cursor-moves
    SOURCES
        cursor_moves.cpp
    LIBRARIES
        EasyLocal::Core
)

# The library in two translation units of one program: no ODR violation.
easylocal_add_test(
    multi-tu
    SOURCES
        odr_main.cpp
        odr_a.cpp
        odr_b.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    cost-expression
    SOURCES
        cost_expression.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    service-base
    SOURCES
        service_base.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    solution-manager-construction
    SOURCES
        solution_manager_construction.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    solution-identity
    SOURCES
        solution_identity.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    tabu-customization
    SOURCES
        tabu_customization.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    neighborhood-parameters
    SOURCES
        neighborhood_parameters.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    concept-contracts
    SOURCES
        concept_contracts.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    testing-checks
    SOURCES
        testing_checks.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    testing-random-draws
    SOURCES
        testing_random_draws.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    assignment-model
    SOURCES
        assignment_model.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    assignment-neighborhood
    SOURCES
        assignment_neighborhood.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    assignment-search
    SOURCES
        assignment_search.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    tsp-model
    SOURCES
        tsp_model.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    tsp-neighborhood
    SOURCES
        tsp_neighborhood.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    tsp-swap-neighborhood
    SOURCES
        tsp_swap_neighborhood.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    evaluation-contract
    SOURCES
        evaluation_contract.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    tsp-evaluation
    SOURCES
        tsp_evaluation.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    tsp-delta
    SOURCES
        tsp_delta.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    approximate-comparison
    SOURCES
        approximate_comparison.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    tsp-floating-point
    SOURCES
        tsp_floating_point.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    exam-timetabling
    SOURCES
        exam_timetabling.cpp
    LIBRARIES
        easylocal_exam_timetabling_model
)

easylocal_add_test(
    simulated-annealing
    SOURCES
        simulated_annealing.cpp
    LIBRARIES
        easylocal_exam_timetabling_model
)

easylocal_add_test(
    hill-climbing
    SOURCES
        hill_climbing.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    late-acceptance-hill-climbing
    SOURCES
        late_acceptance_hill_climbing.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    pareto-cost
    SOURCES
        pareto_cost.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    pareto-late-acceptance-hill-climbing
    SOURCES
        pareto_late_acceptance_hill_climbing.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    great-deluge
    SOURCES
        great_deluge.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    tabu-search
    SOURCES
        tabu_search.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    tabu-lists
    SOURCES
        tabu_lists.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    pfsp
    SOURCES
        pfsp.cpp
    LIBRARIES
        easylocal_pfsp_model
    DEFINITIONS
        EASYLOCAL_PFSP_SMALL_INSTANCE="${PROJECT_SOURCE_DIR}/examples/pfsp/instances/small.pfsp"
        EASYLOCAL_PFSP_MEDIUM_INSTANCE="${PROJECT_SOURCE_DIR}/examples/pfsp/instances/medium.pfsp"
)

easylocal_add_test(
    cost-semantics
    SOURCES
        cost_semantics.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    runner-composition
    SOURCES
        runner_composition.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    service-composition
    SOURCES
        service_composition.cpp
    LIBRARIES
        easylocal_assignment_model
)

easylocal_add_test(
    parameters
    SOURCES
        parameters.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    parameter-set
    SOURCES
        parameter_set.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    parameter-domains
    SOURCES
        parameter_domains.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    parameter-conditions
    SOURCES
        parameter_conditions.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    builtin-parameter-domains
    SOURCES
        builtin_parameter_domains.cpp
    LIBRARIES
        EasyLocal::Core
)


easylocal_add_test(
    configuration-overrides
    SOURCES
        configuration_overrides.cpp
    LIBRARIES
        easylocal_tsp_model
)

easylocal_add_test(
    configuration-file
    SOURCES
        configuration_file.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    configuration-cli
    SOURCES
        configuration_cli.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    configuration-setup
    SOURCES
        configuration_setup.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    neighborhood-composition
    SOURCES
        neighborhood_composition.cpp
    LIBRARIES
        easylocal_assignment_model
)


if(TARGET EasyLocal::TUI)
    easylocal_add_test(
        tui-tester-compile
        SOURCES
            tui_tester_compile.cpp
        LIBRARIES
            easylocal_assignment_model
            EasyLocal::TUI
    )

    easylocal_add_test(
        tui-helpers
        SOURCES
            tui_helpers.cpp
        LIBRARIES
            EasyLocal::TUI
    )


    easylocal_add_test(
        tui-launcher-helpers
        SOURCES
            tui_launcher_helpers.cpp
        LIBRARIES
            EasyLocal::TUI
    )
endif()

if(TARGET EasyLocal::ConfigTOML)
    set(_easylocal_config_toml_enabled ON)
else()
    set(_easylocal_config_toml_enabled OFF)
endif()

if(EASYLOCAL_CONFIG_TOML_BUNDLED)
    set(_easylocal_config_toml_bundled ON)
else()
    set(_easylocal_config_toml_bundled OFF)
endif()

if(TARGET EasyLocal::TUI)
    set(_easylocal_tui_enabled ON)
else()
    set(_easylocal_tui_enabled OFF)
endif()

if(EASYLOCAL_TUI_BUNDLED)
    set(_easylocal_tui_bundled ON)
else()
    set(_easylocal_tui_bundled OFF)
endif()

if(TARGET EasyLocal::REST)
    set(_easylocal_rest_enabled ON)
else()
    set(_easylocal_rest_enabled OFF)
endif()

if(EASYLOCAL_REST_BUNDLED)
    set(_easylocal_rest_bundled ON)
else()
    set(_easylocal_rest_bundled OFF)
endif()

if(EASYLOCAL_REST_ASIO_BUNDLED)
    set(_easylocal_rest_asio_bundled ON)
else()
    set(_easylocal_rest_asio_bundled OFF)
endif()

add_test(
    NAME easylocal.installed-package-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DEASYLOCAL_CONFIG_TOML_ENABLED=${_easylocal_config_toml_enabled}"
        "-DEASYLOCAL_CONFIG_TOML_BUNDLED=${_easylocal_config_toml_bundled}"
        "-DEASYLOCAL_TUI_ENABLED=${_easylocal_tui_enabled}"
        "-DEASYLOCAL_TUI_BUNDLED=${_easylocal_tui_bundled}"
        "-DEASYLOCAL_REST_ENABLED=${_easylocal_rest_enabled}"
        "-DEASYLOCAL_REST_BUNDLED=${_easylocal_rest_bundled}"
        "-DEASYLOCAL_REST_ASIO_BUNDLED=${_easylocal_rest_asio_bundled}"
        "-DEASYLOCAL_DEPENDENCY_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DEASYLOCAL_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DEASYLOCAL_CONFIG=$<CONFIG>"
        "-DEASYLOCAL_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_consumer"
        "-DEASYLOCAL_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
        "-DEASYLOCAL_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DEASYLOCAL_GENERATOR=${CMAKE_GENERATOR}"
        "-DEASYLOCAL_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DEASYLOCAL_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DEASYLOCAL_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DEASYLOCAL_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        "-DEASYLOCAL_VERSION=${PROJECT_VERSION}"
        "-DEASYLOCAL_VERSION_PRERELEASE=${EASYLOCAL_VERSION_PRERELEASE}"
        "-DEASYLOCAL_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/package-consumer"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_installed_package.cmake"
)
set_tests_properties(
    easylocal.installed-package-consumer
    PROPERTIES
        LABELS integration
)

function(easylocal_add_compile_failure_test name source expected_diagnostic)
    set(expected_context "")
    if(ARGC GREATER 3)
        set(expected_context "${ARGV3}")
    endif()

    add_test(
        NAME "easylocal.compile-fail.${name}"
        COMMAND
            "${CMAKE_COMMAND}"
            "-DCXX=${CMAKE_CXX_COMPILER}"
            "-DCXX_ID=${CMAKE_CXX_COMPILER_ID}"
            "-DFRONTEND=${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}"
            # The build's own flags (-stdlib=..., --gcc-install-dir=...) and
            # SDK, so that the test sees the same standard library.
            "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DSYSROOT=${CMAKE_OSX_SYSROOT}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${source}"
            "-DINCLUDE_DIR=${PROJECT_SOURCE_DIR}/include"
            "-DEXPECTED_DIAGNOSTIC=${expected_diagnostic}"
            "-DEXPECTED_CONTEXT=${expected_context}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_failure.cmake"
    )
endfunction()

easylocal_add_compile_failure_test(
    temporary-runner-configuration
    temporary_runner_configuration.cpp
    "configuration"
)
easylocal_add_compile_failure_test(
    temporary-recipe-configuration
    temporary_recipe_configuration.cpp
    "configuration"
)
easylocal_add_compile_failure_test(
    app-pipeline-cost
    app_pipeline_cost.cpp
    "the last stage of a pipeline registered in an app must have the app's cost"
)
easylocal_add_compile_failure_test(
    pipeline-different-solutions
    pipeline_different_solutions.cpp
    "the stages of a pipeline must have the same Solution"
)
easylocal_add_compile_failure_test(
    duplicate-component
    duplicate_component.cpp
    "a SolutionManager recipe may contain each component type at most once"
    "ComponentA"
)
easylocal_add_compile_failure_test(
    session-missing-solution-factory
    session_missing_solution_factory.cpp
    "Session requires the SolutionManager to provide initial_solution() or random_solution(std::mt19937_64&)"
    "SolutionManager"
)
easylocal_add_compile_failure_test(
    missing-solution-manager-inherited-constructor
    missing_solution_manager_inherited_constructor.cpp
    "did you forget `using solution_manager_base::solution_manager_base;`?"
    "BrokenSolutionManager"
)
easylocal_add_compile_failure_test(
    missing-neighborhood-inherited-constructor
    missing_neighborhood_inherited_constructor.cpp
    "did you forget `using neighborhood_explorer_base::neighborhood_explorer_base;`?"
    "BrokenNeighborhood"
)
easylocal_add_compile_failure_test(
    solution-manager-temporary-input
    solution_manager_temporary_input.cpp
    "deleted"
    "solution_manager_base"
)
easylocal_add_compile_failure_test(
    neighborhood-temporary-solution-manager
    neighborhood_temporary_solution_manager.cpp
    "deleted"
    "neighborhood_explorer_base"
)
easylocal_add_compile_failure_test(
    componentless-solution-manager
    componentless_solution_manager.cpp
    "a SolutionManager recipe needs a cost"
    "BaseSolutionManager"
)
easylocal_add_compile_failure_test(
    several-cost-expressions
    several_cost_expressions.cpp
    "a SolutionManager recipe has one cost expression"
    "ComponentB"
)
easylocal_add_compile_failure_test(
    sum-of-domain-values
    sum_of_domain_values.cpp
    "cost::sum adds arithmetic costs"
    "DomainComponent"
)
easylocal_add_compile_failure_test(
    weighted-outside-sum
    weighted_outside_sum.cpp
    "cost::weighted(child, weight) weighs a term of cost::sum"
    "ComponentA"
)
easylocal_add_compile_failure_test(
    duplicate-neighborhood-union-child
    duplicate_neighborhood_union_child.cpp
    "a neighborhood union cannot contain the same NeighborhoodExplorer type more than once"
    "NHE"
)
easylocal_add_compile_failure_test(
    duplicate-delta
    duplicate_delta.cpp
    "a neighborhood recipe may attach at most one delta cost component to each component type"
    "ComponentA"
)
easylocal_add_compile_failure_test(
    unsigned-cost
    unsigned_cost.cpp
    "a cost component cannot return an unsigned integer"
)
easylocal_add_compile_failure_test(
    inactive-delta
    inactive_delta.cpp
    "attached delta names a component that is not active in the bound SolutionManager recipe"
    "ComponentB"
)
easylocal_add_compile_failure_test(
    incompatible-delta
    incompatible_delta.cpp
    "attached delta cost component is incompatible with the bound component value, Solution, or Move type"
    "MalformedDeltaA"
)
easylocal_add_compile_failure_test(
    incompatible-union-child-delta
    incompatible_union_child_delta.cpp
    "attached delta cost component is incompatible with the bound component value, Solution, or Move type"
    "MalformedDeltaA"
)
easylocal_add_compile_failure_test(
    missing-delta-evaluate
    missing_delta_evaluate.cpp
    "attached delta cost component is incompatible with the bound component value, Solution, or Move type"
    "MissingDeltaEvaluateA"
)
easylocal_add_compile_failure_test(
    delta-not-constructible
    delta_not_constructible.cpp
    "a delta cost component must be constructible either from the bound Instance followed by its recipe arguments or from its recipe arguments alone"
    "NonConstructibleDeltaA"
)
easylocal_add_compile_failure_test(
    component-not-constructible
    component_not_constructible.cpp
    "a cost component must be constructible either from the bound Instance followed by its recipe arguments or from its recipe arguments alone"
    "NonConstructibleComponent"
)
easylocal_add_compile_failure_test(
    simulated-annealing-non-difference-cost
    simulated_annealing_non_difference_cost.cpp
    "MetropolisAcceptance requires candidate_cost - current_cost"
    "MetropolisAcceptance"
)
easylocal_add_compile_failure_test(
    duplicate-parameter-field-name
    duplicate_parameter_field_name.cpp
    "parameter fields must have unique names within a block"
    "duplicate"
)
easylocal_add_compile_failure_test(
    parameter-domain-mismatch
    parameter_domain_mismatch.cpp
    "the domain does not fit the field"
    "policy"
)
easylocal_add_compile_failure_test(
    parameter-condition-unknown-path
    parameter_condition_unknown_path.cpp
    "no field or group of the block has this name"
    "sample"
)

# The code of docs/quick-start.md and docs/tutorial is built and run here, so
# the documentation cannot drift from the library.
if(TARGET easylocal_quickstart)
    add_test(NAME easylocal.example.quickstart COMMAND easylocal_quickstart)
    set_tests_properties(
        easylocal.example.quickstart
        PROPERTIES PASS_REGULAR_EXPRESSION "length 26 after 14 evaluations")
endif()

# The documentation's snippets must match the examples they are taken from.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME easylocal.docs.snippets
        COMMAND Python3::Interpreter
            ${PROJECT_SOURCE_DIR}/scripts/sync-doc-snippets.py --check)
endif()

if(TARGET easylocal_tutorial_toml)
    add_test(NAME easylocal.example.tutorial-toml COMMAND easylocal_tutorial_toml)
    set_tests_properties(
        easylocal.example.tutorial-toml
        PROPERTIES PASS_REGULAR_EXPRESSION "2 parameter blocks from the file\ncost ")
endif()

if(TARGET easylocal_tutorial)
    add_test(NAME easylocal.example.tutorial COMMAND easylocal_tutorial)
    set_tests_properties(
        easylocal.example.tutorial
        PROPERTIES PASS_REGULAR_EXPRESSION "first improvement 26\n")
    add_test(NAME easylocal.example.tutorial-checks COMMAND easylocal_tutorial_checks)
    add_test(
        NAME easylocal.example.tutorial-cli
        COMMAND easylocal_tutorial_cli
            --instance ${PROJECT_SOURCE_DIR}/examples/tutorial/five.tsp --seed 1 --runner fi)
    set_tests_properties(
        easylocal.example.tutorial-cli
        PROPERTIES PASS_REGULAR_EXPRESSION "cost [0-9]+\ntime ")
    add_test(
        NAME easylocal.example.tutorial-staged
        COMMAND easylocal_tutorial_staged --main.seed 1 --main.evaluations_per_city 20)
    set_tests_properties(
        easylocal.example.tutorial-staged
        PROPERTIES PASS_REGULAR_EXPRESSION "violations 0, length [0-9]+\n")
endif()

if(TARGET easylocal_tutorial_pipeline)
    add_test(
        NAME easylocal.example.tutorial-pipeline
        COMMAND easylocal_tutorial_pipeline)
    set_tests_properties(
        easylocal.example.tutorial-pipeline
        PROPERTIES PASS_REGULAR_EXPRESSION "stage feasible: [0-9]+ attempt.*\nstage climb: .*\nviolations 0, length [0-9]+")
endif()

if(TARGET easylocal_pfsp_tabu)
    add_test(
        NAME easylocal.example.pfsp-tabu
        COMMAND easylocal_pfsp_tabu --runners.tabu.max_iterations=50)
    set_tests_properties(
        easylocal.example.pfsp-tabu
        PROPERTIES PASS_REGULAR_EXPRESSION "\niterations 50\n")
endif()

if(TARGET easylocal_assignment)
    add_test(
        NAME easylocal.example.assignment-cli-help
        COMMAND easylocal_assignment --help
    )
    set_tests_properties(
        easylocal.example.assignment-cli-help
        PROPERTIES
            PASS_REGULAR_EXPRESSION "--runners.fi.max_evaluations <value>"
    )

    add_test(
        NAME easylocal.example.assignment-cli-override
        COMMAND easylocal_assignment --runners.fi.max_evaluations=1
    )
    set_tests_properties(
        easylocal.example.assignment-cli-override
        PROPERTIES
            PASS_REGULAR_EXPRESSION "termination evaluation budget exhausted"
    )

    add_test(
        NAME easylocal.example.assignment-config-file
        COMMAND easylocal_assignment
            --config ${PROJECT_SOURCE_DIR}/examples/assignment/configs/small.cfg
    )
    set_tests_properties(
        easylocal.example.assignment-config-file
        PROPERTIES
            PASS_REGULAR_EXPRESSION "termination evaluation budget exhausted"
    )
endif()

if(TARGET easylocal_exam_timetabling)
    add_test(
        NAME easylocal.example.exam-cli-help
        COMMAND easylocal_exam_timetabling --help
    )
    set_tests_properties(
        easylocal.example.exam-cli-help
        PROPERTIES
            PASS_REGULAR_EXPRESSION "--runners.sa.temperature.cooling_rate <value>"
    )

    add_test(
        NAME easylocal.example.exam-cli-override
        COMMAND easylocal_exam_timetabling --runners.sa.temperature.max_iterations=1
    )
    set_tests_properties(
        easylocal.example.exam-cli-override
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\niterations 1\n"
    )

    add_test(
        NAME easylocal.example.exam-config-file
        COMMAND easylocal_exam_timetabling
            --config ${PROJECT_SOURCE_DIR}/examples/exam_timetabling/configs/small.cfg
    )
    set_tests_properties(
        easylocal.example.exam-config-file
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\niterations 1\n"
    )
endif()

if(TARGET easylocal_tsp_sa)
    add_test(
        NAME easylocal.example.tsp-cli-help
        COMMAND easylocal_tsp_sa --help
    )
    set_tests_properties(
        easylocal.example.tsp-cli-help
        PROPERTIES
            PASS_REGULAR_EXPRESSION "--neighborhood.random_biases <value>"
    )

    add_test(
        NAME easylocal.example.tsp-cli-override
        COMMAND easylocal_tsp_sa --runners.sa.temperature.max_iterations=1
    )
    set_tests_properties(
        easylocal.example.tsp-cli-override
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\niterations 1\n"
    )

    add_test(
        NAME easylocal.example.tsp-config-file
        COMMAND easylocal_tsp_sa
            --config ${PROJECT_SOURCE_DIR}/examples/tsp/configs/small.cfg
    )
    set_tests_properties(
        easylocal.example.tsp-config-file
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\niterations 1\n"
    )

    add_test(
        NAME easylocal.example.tsp-config-precedence
        COMMAND easylocal_tsp_sa
            --config ${PROJECT_SOURCE_DIR}/examples/tsp/configs/small.cfg
            --runners.sa.temperature.max_iterations=2
    )
    set_tests_properties(
        easylocal.example.tsp-config-precedence
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\niterations 2\n"
    )

    add_test(
        NAME easylocal.example.tsp-config-error
        COMMAND easylocal_tsp_sa
            --config ${PROJECT_SOURCE_DIR}/examples/tsp/configs/invalid.cfg
    )
    set_tests_properties(
        easylocal.example.tsp-config-error
        PROPERTIES
            WILL_FAIL TRUE
    )

    add_test(
        NAME easylocal.example.tsp-cli-error
        COMMAND easylocal_tsp_sa
            --runners.sa.temperature.cooling_rate=1.5
    )
    set_tests_properties(
        easylocal.example.tsp-cli-error
        PROPERTIES
            WILL_FAIL TRUE
    )
endif()

easylocal_add_test(
    solver
    SOURCES
        solver.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    multi-start-solver
    SOURCES
        multi_start_solver.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    cli-run
    SOURCES
        cli_run.cpp
    LIBRARIES
        EasyLocal::Core
    DEFINITIONS
        EASYLOCAL_TUTORIAL_INSTANCE="${PROJECT_SOURCE_DIR}/examples/tutorial/five.tsp"
)

easylocal_add_test(
    reproducibility
    SOURCES
        reproducibility.cpp
    LIBRARIES
        EasyLocal::Core
    DEFINITIONS
        EASYLOCAL_TUTORIAL_INSTANCE="${PROJECT_SOURCE_DIR}/examples/tutorial/five.tsp"
)

easylocal_add_test(
    tuning
    SOURCES
        tuning.cpp
    LIBRARIES
        EasyLocal::Core
    DEFINITIONS
        EASYLOCAL_TUTORIAL_INSTANCE="${PROJECT_SOURCE_DIR}/examples/tutorial/five.tsp"
)

# irace on the TSP example: the scenario it writes, checked and run briefly.
# Skipped without R and its irace package, and on Windows, where the target
# runner (a shell script) does not run.
if(TARGET easylocal_tsp_sa AND NOT WIN32)
    add_test(
        NAME easylocal.irace-smoke
        COMMAND
            "${CMAKE_COMMAND}"
            "-DEASYLOCAL_PROGRAM=$<TARGET_FILE:easylocal_tsp_sa>"
            "-DEASYLOCAL_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/irace-smoke"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/irace_smoke.cmake")
    set_tests_properties(
        easylocal.irace-smoke
        PROPERTIES
            LABELS "integration;irace"
            SKIP_REGULAR_EXPRESSION "SKIPPED: irace is not available"
            TIMEOUT 600)
endif()

easylocal_add_test(
    run-limits
    SOURCES
        run_limits.cpp
    LIBRARIES
        EasyLocal::Core
)
easylocal_add_test(
    pipeline
    SOURCES
        pipeline.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    run-target
    SOURCES
        run_target.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    app
    SOURCES
        app.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    app-graph-shape
    SOURCES
        app_graph_shape.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    app-check
    SOURCES
        app_check.cpp
    LIBRARIES
        EasyLocal::Core
)

if(TARGET easylocal_tsp_two_apps)
    add_test(
        NAME easylocal.example.tsp-two-apps
        COMMAND easylocal_tsp_two_apps
    )
endif()

easylocal_add_test(
    stochastic-tools
    SOURCES
        stochastic_tools.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    session
    SOURCES
        session.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    session-configuration
    SOURCES
        session_configuration.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    cost-text
    SOURCES
        cost_text.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    union-biases-lookup
    SOURCES
        union_biases_lookup.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    union-moves
    SOURCES
        union_moves.cpp
    LIBRARIES
        EasyLocal::Core
)

easylocal_add_test(
    session-io
    SOURCES
        session_io.cpp
    LIBRARIES
        EasyLocal::Core
)

if(TARGET EasyLocal::REST)
    easylocal_add_test(
        rest-execution
        SOURCES
            rest_execution.cpp
        LIBRARIES
            EasyLocal::REST
    )

    easylocal_add_test(
        rest-blueprint-requests
        SOURCES
            rest_blueprint_requests.cpp
        LIBRARIES
            EasyLocal::REST
            easylocal_assignment_model
    )
    set_tests_properties(
        easylocal.rest-blueprint-requests
        PROPERTIES
            TIMEOUT 60
    )

    find_program(EASYLOCAL_CURL_EXECUTABLE NAMES curl)
    if(UNIX AND EASYLOCAL_CURL_EXECUTABLE AND TARGET easylocal_assignment_rest)
        add_test(
            NAME easylocal.rest-http
            COMMAND
                "${CMAKE_CURRENT_SOURCE_DIR}/rest_http_integration.sh"
                "$<TARGET_FILE:easylocal_assignment_rest>"
                "${EASYLOCAL_CURL_EXECUTABLE}"
        )
        set_tests_properties(
            easylocal.rest-http
            PROPERTIES
                LABELS "integration;rest-http"
                TIMEOUT 60
        )
        if(TARGET easylocal_tutorial_rest)
            add_test(
                NAME easylocal.example.tutorial-rest
                COMMAND
                    "${CMAKE_CURRENT_SOURCE_DIR}/tutorial_rest_integration.sh"
                    "$<TARGET_FILE:easylocal_tutorial_rest>"
                    "${EASYLOCAL_CURL_EXECUTABLE}"
            )
            set_tests_properties(
                easylocal.example.tutorial-rest
                PROPERTIES
                    LABELS "integration;rest-http"
                    TIMEOUT 60
            )
        endif()
    elseif(NOT EASYLOCAL_CURL_EXECUTABLE)
        message(STATUS "curl not found: REST HTTP integration test disabled")
    endif()
endif()

# End-to-end tests of the tutorial's TextUI in a pseudo-terminal (tests/tui),
# with pytest and pyte from the uv environment, or from the Python found.
if(UNIX AND TARGET easylocal_tutorial_tui)
    find_program(EASYLOCAL_UV_EXECUTABLE NAMES uv)
    set(easylocal_tui_e2e_python "")
    if(EASYLOCAL_UV_EXECUTABLE)
        set(easylocal_tui_e2e_python
            "${EASYLOCAL_UV_EXECUTABLE}" run --project "${PROJECT_SOURCE_DIR}" --frozen python)
    elseif(Python3_Interpreter_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c "import pyte, pytest"
            RESULT_VARIABLE easylocal_tui_e2e_modules
            OUTPUT_QUIET ERROR_QUIET)
        if(easylocal_tui_e2e_modules EQUAL 0)
            set(easylocal_tui_e2e_python "${Python3_EXECUTABLE}")
        endif()
    endif()

    if(easylocal_tui_e2e_python)
        set(easylocal_tui_e2e_environment
            "EASYLOCAL_TUTORIAL_TUI=$<TARGET_FILE:easylocal_tutorial_tui>")
        if(TARGET easylocal_tsp_tui)  # the launcher of two apps
            list(APPEND easylocal_tui_e2e_environment
                "EASYLOCAL_TSP_TUI=$<TARGET_FILE:easylocal_tsp_tui>")
        endif()
        if(TARGET easylocal_assignment_tui)  # its slow-fi runner can be stopped
            list(APPEND easylocal_tui_e2e_environment
                "EASYLOCAL_ASSIGNMENT_TUI=$<TARGET_FILE:easylocal_assignment_tui>")
        endif()
        # One test per file, easylocal.tui-e2e.<name> for test_<name>.py, so
        # that ctest runs them in parallel.
        file(GLOB easylocal_tui_e2e_files CONFIGURE_DEPENDS
            "${PROJECT_SOURCE_DIR}/tests/tui/test_*.py")
        foreach(easylocal_tui_e2e_file IN LISTS easylocal_tui_e2e_files)
            get_filename_component(easylocal_tui_e2e_name "${easylocal_tui_e2e_file}" NAME_WE)
            string(REGEX REPLACE "^test_" "" easylocal_tui_e2e_name "${easylocal_tui_e2e_name}")
            add_test(
                NAME easylocal.tui-e2e.${easylocal_tui_e2e_name}
                COMMAND ${easylocal_tui_e2e_python} -m pytest -q -p no:cacheprovider
                    "${easylocal_tui_e2e_file}"
            )
            set_tests_properties(
                easylocal.tui-e2e.${easylocal_tui_e2e_name}
                PROPERTIES
                    LABELS "integration;tui-e2e"
                    ENVIRONMENT "${easylocal_tui_e2e_environment}"
                    TIMEOUT 300
            )
        endforeach()
    else()
        message(STATUS "neither uv nor Python with pyte and pytest: TUI end-to-end tests disabled")
    endif()
endif()

easylocal_add_test(
    trace
    SOURCES
        trace.cpp
    LIBRARIES
        EasyLocal::Core
)

# The ELTR decoder (scripts/eltr.py) reads what the binary recorder writes as
# the JSONL recorder writes it.
add_executable(
    easylocal_eltr_fixture
    eltr_fixture.cpp
)
target_link_libraries(
    easylocal_eltr_fixture
    PRIVATE
        EasyLocal::Core
)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME easylocal.trace.eltr-decode
        COMMAND Python3::Interpreter
            ${CMAKE_CURRENT_SOURCE_DIR}/eltr_decode.py
            $<TARGET_FILE:easylocal_eltr_fixture>)
endif()

# A test that hangs fails after two minutes, under its own name, rather than
# at the CI job's time limit; the tests that take longer set their own.
get_property(easylocal_tests DIRECTORY PROPERTY TESTS)
foreach(easylocal_test IN LISTS easylocal_tests)
    get_test_property(${easylocal_test} TIMEOUT easylocal_timeout)
    if(NOT easylocal_timeout)
        set_tests_properties(${easylocal_test} PROPERTIES TIMEOUT 120)
    endif()
endforeach()
