easylocal_enable_strict_warnings()

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

add_executable(
    easylocal_toolchain_smoke
    toolchain_smoke.cpp
)
target_link_libraries(
    easylocal_toolchain_smoke
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.toolchain
    COMMAND easylocal_toolchain_smoke
)



if(TARGET EasyLocal::ConfigTOML)
    add_executable(
        easylocal_configuration_toml_test
        configuration_toml.cpp
    )
    target_link_libraries(
        easylocal_configuration_toml_test
        PRIVATE
            EasyLocal::ConfigTOML
    )
    add_test(
        NAME easylocal.configuration-toml
        COMMAND easylocal_configuration_toml_test
    )
endif()

add_executable(
    easylocal_factory_api_test
    factory_api.cpp
)
target_link_libraries(
    easylocal_factory_api_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.factory-api
    COMMAND easylocal_factory_api_test
)

add_executable(
    easylocal_logging_test
    logging.cpp
)
target_link_libraries(
    easylocal_logging_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.logging
    COMMAND easylocal_logging_test
)

add_executable(
    easylocal_run_control_test
    run_control.cpp
)
target_link_libraries(
    easylocal_run_control_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.run-control
    COMMAND easylocal_run_control_test
)

add_executable(
    easylocal_cursor_moves_test
    cursor_moves.cpp
)
target_link_libraries(
    easylocal_cursor_moves_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.cursor-moves
    COMMAND easylocal_cursor_moves_test
)

add_executable(
    easylocal_multi_tu
    odr_main.cpp
    odr_a.cpp
    odr_b.cpp
)
target_link_libraries(
    easylocal_multi_tu
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.multi-tu
    COMMAND easylocal_multi_tu
)

add_executable(
    easylocal_cost_expression_test
    cost_expression.cpp
)
target_link_libraries(
    easylocal_cost_expression_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.cost_expression
    COMMAND easylocal_cost_expression_test
)

add_executable(
    easylocal_service_base_test
    service_base.cpp
)
target_link_libraries(
    easylocal_service_base_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.service-base
    COMMAND easylocal_service_base_test
)

add_executable(
    easylocal_solution_manager_construction_test
    solution_manager_construction.cpp
)
target_link_libraries(
    easylocal_solution_manager_construction_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.solution-manager-construction
    COMMAND easylocal_solution_manager_construction_test
)

add_executable(
    easylocal_solution_identity_test
    solution_identity.cpp
)
target_link_libraries(
    easylocal_solution_identity_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.solution-identity
    COMMAND easylocal_solution_identity_test
)

add_executable(
    easylocal_tabu_customization_test
    tabu_customization.cpp
)
target_link_libraries(
    easylocal_tabu_customization_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.tabu-customization
    COMMAND easylocal_tabu_customization_test
)

add_executable(
    easylocal_neighborhood_parameters_test
    neighborhood_parameters.cpp
)
target_link_libraries(
    easylocal_neighborhood_parameters_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.neighborhood-parameters
    COMMAND easylocal_neighborhood_parameters_test
)

add_executable(
    easylocal_concept_contracts_test
    concept_contracts.cpp
)
target_link_libraries(
    easylocal_concept_contracts_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.concept-contracts
    COMMAND easylocal_concept_contracts_test
)

add_executable(
    easylocal_testing_checks_test
    testing_checks.cpp
)
target_link_libraries(
    easylocal_testing_checks_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.testing-checks
    COMMAND easylocal_testing_checks_test
)

add_executable(
    easylocal_assignment_model_test
    assignment_model.cpp
)
target_link_libraries(
    easylocal_assignment_model_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.assignment-model
    COMMAND easylocal_assignment_model_test
)

add_executable(
    easylocal_assignment_neighborhood_test
    assignment_neighborhood.cpp
)
target_link_libraries(
    easylocal_assignment_neighborhood_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.assignment-neighborhood
    COMMAND easylocal_assignment_neighborhood_test
)

add_executable(
    easylocal_assignment_search_test
    assignment_search.cpp
)
target_link_libraries(
    easylocal_assignment_search_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.assignment-search
    COMMAND easylocal_assignment_search_test
)

add_executable(
    easylocal_tsp_model_test
    tsp_model.cpp
)
target_link_libraries(
    easylocal_tsp_model_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.tsp-model
    COMMAND easylocal_tsp_model_test
)

add_executable(
    easylocal_tsp_neighborhood_test
    tsp_neighborhood.cpp
)
target_link_libraries(
    easylocal_tsp_neighborhood_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.tsp-neighborhood
    COMMAND easylocal_tsp_neighborhood_test
)

add_executable(
    easylocal_tsp_swap_neighborhood_test
    tsp_swap_neighborhood.cpp
)
target_link_libraries(
    easylocal_tsp_swap_neighborhood_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.tsp-swap-neighborhood
    COMMAND easylocal_tsp_swap_neighborhood_test
)

add_executable(
    easylocal_evaluation_contract_test
    evaluation_contract.cpp
)
target_link_libraries(
    easylocal_evaluation_contract_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.evaluation-contract
    COMMAND easylocal_evaluation_contract_test
)

add_executable(
    easylocal_tsp_evaluation_test
    tsp_evaluation.cpp
)
target_link_libraries(
    easylocal_tsp_evaluation_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.tsp-evaluation
    COMMAND easylocal_tsp_evaluation_test
)

add_executable(
    easylocal_tsp_delta_test
    tsp_delta.cpp
)
target_link_libraries(
    easylocal_tsp_delta_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.tsp-delta
    COMMAND easylocal_tsp_delta_test
)

add_executable(
    easylocal_approximate_comparison_test
    approximate_comparison.cpp
)
target_link_libraries(
    easylocal_approximate_comparison_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.approximate-comparison
    COMMAND easylocal_approximate_comparison_test
)

add_executable(
    easylocal_tsp_floating_point_test
    tsp_floating_point.cpp
)
target_link_libraries(
    easylocal_tsp_floating_point_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.tsp-floating-point
    COMMAND easylocal_tsp_floating_point_test
)

add_executable(
    easylocal_exam_timetabling_test
    exam_timetabling.cpp
)
target_link_libraries(
    easylocal_exam_timetabling_test
    PRIVATE
        easylocal_exam_timetabling_mwe_model
)
add_test(
    NAME easylocal.exam-timetabling
    COMMAND easylocal_exam_timetabling_test
)

add_executable(
    easylocal_simulated_annealing_test
    simulated_annealing.cpp
)
target_link_libraries(
    easylocal_simulated_annealing_test
    PRIVATE
        easylocal_exam_timetabling_mwe_model
)
add_test(
    NAME easylocal.simulated-annealing
    COMMAND easylocal_simulated_annealing_test
)

add_executable(
    easylocal_hill_climbing_test
    hill_climbing.cpp
)
target_link_libraries(
    easylocal_hill_climbing_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.hill-climbing
    COMMAND easylocal_hill_climbing_test
)

add_executable(
    easylocal_late_acceptance_hill_climbing_test
    late_acceptance_hill_climbing.cpp
)
target_link_libraries(
    easylocal_late_acceptance_hill_climbing_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.late-acceptance-hill-climbing
    COMMAND easylocal_late_acceptance_hill_climbing_test
)

add_executable(
    easylocal_pareto_cost_test
    pareto_cost.cpp
)
target_link_libraries(
    easylocal_pareto_cost_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.pareto-cost
    COMMAND easylocal_pareto_cost_test
)

add_executable(
    easylocal_pareto_late_acceptance_hill_climbing_test
    pareto_late_acceptance_hill_climbing.cpp
)
target_link_libraries(
    easylocal_pareto_late_acceptance_hill_climbing_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.pareto-late-acceptance-hill-climbing
    COMMAND easylocal_pareto_late_acceptance_hill_climbing_test
)

add_executable(
    easylocal_great_deluge_test
    great_deluge.cpp
)
target_link_libraries(
    easylocal_great_deluge_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.great-deluge
    COMMAND easylocal_great_deluge_test
)

add_executable(
    easylocal_tabu_search_test
    tabu_search.cpp
)
target_link_libraries(
    easylocal_tabu_search_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.tabu-search
    COMMAND easylocal_tabu_search_test
)

add_executable(
    easylocal_tabu_lists_test
    tabu_lists.cpp
)
target_link_libraries(
    easylocal_tabu_lists_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.tabu-lists
    COMMAND easylocal_tabu_lists_test
)

add_executable(
    easylocal_pfsp_test
    pfsp.cpp
)
target_link_libraries(
    easylocal_pfsp_test
    PRIVATE
        easylocal_pfsp_model
)
target_compile_definitions(
    easylocal_pfsp_test
    PRIVATE
        EASYLOCAL_PFSP_SMALL_INSTANCE="${PROJECT_SOURCE_DIR}/examples/pfsp/instances/small.pfsp"
        EASYLOCAL_PFSP_MEDIUM_INSTANCE="${PROJECT_SOURCE_DIR}/examples/pfsp/instances/medium.pfsp"
)
add_test(
    NAME easylocal.pfsp
    COMMAND easylocal_pfsp_test
)

add_executable(
    easylocal_cost_semantics_test
    cost_semantics.cpp
)
target_link_libraries(
    easylocal_cost_semantics_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.cost-semantics
    COMMAND easylocal_cost_semantics_test
)

add_executable(
    easylocal_runner_composition_test
    runner_composition.cpp
)
target_link_libraries(
    easylocal_runner_composition_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.runner-composition
    COMMAND easylocal_runner_composition_test
)

add_executable(
    easylocal_service_composition_test
    service_composition.cpp
)
target_link_libraries(
    easylocal_service_composition_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.service-composition
    COMMAND easylocal_service_composition_test
)

add_executable(
    easylocal_parameters_test
    parameters.cpp
)
target_link_libraries(
    easylocal_parameters_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.parameters
    COMMAND easylocal_parameters_test
)

add_executable(
    easylocal_parameter_set_test
    parameter_set.cpp
)
target_link_libraries(
    easylocal_parameter_set_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.parameter-set
    COMMAND easylocal_parameter_set_test
)


add_executable(
    easylocal_configuration_overrides_test
    configuration_overrides.cpp
)
target_link_libraries(
    easylocal_configuration_overrides_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.configuration-overrides
    COMMAND easylocal_configuration_overrides_test
)

add_executable(
    easylocal_configuration_file_test
    configuration_file.cpp
)
target_link_libraries(
    easylocal_configuration_file_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-file
    COMMAND easylocal_configuration_file_test
)

add_executable(
    easylocal_configuration_cli_test
    configuration_cli.cpp
)
target_link_libraries(
    easylocal_configuration_cli_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-cli
    COMMAND easylocal_configuration_cli_test
)

add_executable(
    easylocal_configuration_setup_test
    configuration_setup.cpp
)
target_link_libraries(
    easylocal_configuration_setup_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-setup
    COMMAND easylocal_configuration_setup_test
)

add_executable(
    easylocal_neighborhood_composition_test
    neighborhood_composition.cpp
)
target_link_libraries(
    easylocal_neighborhood_composition_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.neighborhood-composition
    COMMAND easylocal_neighborhood_composition_test
)


if(TARGET EasyLocal::TUI)
    add_executable(
        easylocal_tui_tester_compile_test
        tui_tester_compile.cpp
    )
    target_link_libraries(
        easylocal_tui_tester_compile_test
        PRIVATE
            easylocal_assignment_mwe_model
            EasyLocal::TUI
    )
    add_test(
        NAME easylocal.tui-tester-compile
        COMMAND easylocal_tui_tester_compile_test
    )

    add_executable(
        easylocal_tui_helpers_test
        tui_helpers.cpp
    )
    target_link_libraries(
        easylocal_tui_helpers_test
        PRIVATE
            EasyLocal::TUI
    )
    add_test(
        NAME easylocal.tui-helpers
        COMMAND easylocal_tui_helpers_test
    )


    add_executable(
        easylocal_tui_launcher_helpers_test
        tui_launcher_helpers.cpp
    )
    target_link_libraries(
        easylocal_tui_launcher_helpers_test
        PRIVATE
            EasyLocal::TUI
    )
    add_test(
        NAME easylocal.tui-launcher-helpers
        COMMAND easylocal_tui_launcher_helpers_test
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
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${source}"
            "-DINCLUDE_DIR=${PROJECT_SOURCE_DIR}/include"
            "-DEXPECTED_DIAGNOSTIC=${expected_diagnostic}"
            "-DEXPECTED_CONTEXT=${expected_context}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_failure.cmake"
    )
endfunction()

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
    "a neighborhood recipe may attach at most one delta evaluator to each component type"
    "ComponentA"
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
    "attached delta evaluator is incompatible with the bound component value, Solution, or Move type"
    "MalformedDeltaA"
)
easylocal_add_compile_failure_test(
    incompatible-union-child-delta
    incompatible_union_child_delta.cpp
    "attached delta evaluator is incompatible with the bound component value, Solution, or Move type"
    "MalformedDeltaA"
)
easylocal_add_compile_failure_test(
    missing-delta-evaluate
    missing_delta_evaluate.cpp
    "attached delta evaluator is incompatible with the bound component value, Solution, or Move type"
    "MissingDeltaEvaluateA"
)
easylocal_add_compile_failure_test(
    delta-not-constructible
    delta_not_constructible.cpp
    "a delta evaluator must be constructible either from the bound Instance followed by its recipe arguments or from its recipe arguments alone"
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
    simulated_annealing_non_numeric_cost.cpp
    "MetropolisAcceptance requires candidate_cost - current_cost"
    "MetropolisAcceptance"
)
easylocal_add_compile_failure_test(
    duplicate-parameter-field-name
    duplicate_parameter_field_name.cpp
    "parameter fields must have unique names within a block"
    "duplicate"
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

if(TARGET easylocal_assignment_mwe)
    add_test(
        NAME easylocal.example.assignment-cli-help
        COMMAND easylocal_assignment_mwe --help
    )
    set_tests_properties(
        easylocal.example.assignment-cli-help
        PROPERTIES
            PASS_REGULAR_EXPRESSION "--solver.search.max_evaluations <value>"
    )

    add_test(
        NAME easylocal.example.assignment-cli-override
        COMMAND easylocal_assignment_mwe --solver.search.max_evaluations=1
    )
    set_tests_properties(
        easylocal.example.assignment-cli-override
        PROPERTIES
            PASS_REGULAR_EXPRESSION "evaluation budget exhausted"
    )

    add_test(
        NAME easylocal.example.assignment-config-file
        COMMAND easylocal_assignment_mwe
            --config ${PROJECT_SOURCE_DIR}/examples/assignment/configs/small.cfg
    )
    set_tests_properties(
        easylocal.example.assignment-config-file
        PROPERTIES
            PASS_REGULAR_EXPRESSION "evaluation budget exhausted"
    )
endif()

if(TARGET easylocal_exam_timetabling_mwe)
    add_test(
        NAME easylocal.example.exam-cli-help
        COMMAND easylocal_exam_timetabling_mwe --help
    )
    set_tests_properties(
        easylocal.example.exam-cli-help
        PROPERTIES
            PASS_REGULAR_EXPRESSION "--solver.search.temperature.cooling_rate <value>"
    )

    add_test(
        NAME easylocal.example.exam-cli-override
        COMMAND easylocal_exam_timetabling_mwe --solver.search.temperature.max_iterations=1
    )
    set_tests_properties(
        easylocal.example.exam-cli-override
        PROPERTIES
            PASS_REGULAR_EXPRESSION "iterations: 1"
    )

    add_test(
        NAME easylocal.example.exam-config-file
        COMMAND easylocal_exam_timetabling_mwe
            --config ${PROJECT_SOURCE_DIR}/examples/exam_timetabling/configs/small.cfg
    )
    set_tests_properties(
        easylocal.example.exam-config-file
        PROPERTIES
            PASS_REGULAR_EXPRESSION "iterations: 1"
    )
endif()

if(TARGET easylocal_tsp_sa_mwe)
    add_test(
        NAME easylocal.example.tsp-cli-help
        COMMAND easylocal_tsp_sa_mwe --help
    )
    set_tests_properties(
        easylocal.example.tsp-cli-help
        PROPERTIES
            PASS_REGULAR_EXPRESSION "--solver.neighborhood.random_biases <value>"
    )

    add_test(
        NAME easylocal.example.tsp-cli-override
        COMMAND easylocal_tsp_sa_mwe --solver.search.temperature.max_iterations=1
    )
    set_tests_properties(
        easylocal.example.tsp-cli-override
        PROPERTIES
            PASS_REGULAR_EXPRESSION "iterations: 1"
    )

    add_test(
        NAME easylocal.example.tsp-config-file
        COMMAND easylocal_tsp_sa_mwe
            --config ${PROJECT_SOURCE_DIR}/examples/tsp/configs/small.cfg
    )
    set_tests_properties(
        easylocal.example.tsp-config-file
        PROPERTIES
            PASS_REGULAR_EXPRESSION "iterations: 1"
    )

    add_test(
        NAME easylocal.example.tsp-config-precedence
        COMMAND easylocal_tsp_sa_mwe
            --config ${PROJECT_SOURCE_DIR}/examples/tsp/configs/small.cfg
            --solver.search.temperature.max_iterations=2
    )
    set_tests_properties(
        easylocal.example.tsp-config-precedence
        PROPERTIES
            PASS_REGULAR_EXPRESSION "iterations: 2"
    )

    add_test(
        NAME easylocal.example.tsp-config-error
        COMMAND easylocal_tsp_sa_mwe
            --config ${PROJECT_SOURCE_DIR}/examples/tsp/configs/invalid.cfg
    )
    set_tests_properties(
        easylocal.example.tsp-config-error
        PROPERTIES
            WILL_FAIL TRUE
    )

    add_test(
        NAME easylocal.example.tsp-cli-error
        COMMAND easylocal_tsp_sa_mwe
            --solver.search.temperature.cooling_rate=1.5
    )
    set_tests_properties(
        easylocal.example.tsp-cli-error
        PROPERTIES
            WILL_FAIL TRUE
    )
endif()

add_executable(easylocal_solver_test solver.cpp)
target_link_libraries(easylocal_solver_test PRIVATE EasyLocal::Core)
add_test(NAME easylocal.solver COMMAND easylocal_solver_test)

add_executable(easylocal_multi_start_solver_test multi_start_solver.cpp)
target_link_libraries(easylocal_multi_start_solver_test PRIVATE EasyLocal::Core)
add_test(NAME easylocal.multi-start-solver COMMAND easylocal_multi_start_solver_test)

add_executable(easylocal_cli_run_test cli_run.cpp)
target_link_libraries(easylocal_cli_run_test PRIVATE EasyLocal::Core)
target_compile_definitions(
    easylocal_cli_run_test
    PRIVATE EASYLOCAL_TUTORIAL_INSTANCE="${PROJECT_SOURCE_DIR}/examples/tutorial/five.tsp")
add_test(NAME easylocal.cli-run COMMAND easylocal_cli_run_test)

add_executable(easylocal_two_stage_solver_test two_stage_solver.cpp)
target_link_libraries(easylocal_two_stage_solver_test PRIVATE EasyLocal::Core)
add_test(NAME easylocal.two-stage-solver COMMAND easylocal_two_stage_solver_test)

add_executable(easylocal_run_target_test run_target.cpp)
target_link_libraries(easylocal_run_target_test PRIVATE EasyLocal::Core)
add_test(NAME easylocal.run-target COMMAND easylocal_run_target_test)

add_executable(
    easylocal_app_test
    app.cpp
)
target_link_libraries(
    easylocal_app_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.app
    COMMAND easylocal_app_test
)

add_executable(
    easylocal_app_graph_shape_test
    app_graph_shape.cpp
)
target_link_libraries(
    easylocal_app_graph_shape_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.app-graph-shape
    COMMAND easylocal_app_graph_shape_test
)

add_executable(
    easylocal_app_check_test
    app_check.cpp
)
target_link_libraries(
    easylocal_app_check_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.app-check
    COMMAND easylocal_app_check_test
)

if(TARGET easylocal_tsp_two_apps_mwe)
    add_test(
        NAME easylocal.example.tsp-two-apps
        COMMAND easylocal_tsp_two_apps_mwe
    )
endif()

add_executable(
    easylocal_stochastic_tools_test
    stochastic_tools.cpp
)
target_link_libraries(
    easylocal_stochastic_tools_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.stochastic-tools
    COMMAND easylocal_stochastic_tools_test
)

add_executable(
    easylocal_session_test
    session.cpp
)
target_link_libraries(
    easylocal_session_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.session
    COMMAND easylocal_session_test
)

add_executable(
    easylocal_session_configuration_test
    session_configuration.cpp
)
target_link_libraries(
    easylocal_session_configuration_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.session-configuration
    COMMAND easylocal_session_configuration_test
)

add_executable(
    easylocal_cost_text_test
    cost_text.cpp
)
target_link_libraries(
    easylocal_cost_text_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.cost-text
    COMMAND easylocal_cost_text_test
)

add_executable(
    easylocal_union_moves_test
    union_moves.cpp
)
target_link_libraries(
    easylocal_union_moves_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.union-moves
    COMMAND easylocal_union_moves_test
)

add_executable(
    easylocal_session_io_test
    session_io.cpp
)
target_link_libraries(
    easylocal_session_io_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.session-io
    COMMAND easylocal_session_io_test
)

if(TARGET EasyLocal::REST)
    add_executable(
        easylocal_rest_execution_test
        rest_execution.cpp
    )
    target_link_libraries(
        easylocal_rest_execution_test
        PRIVATE
            EasyLocal::REST
    )
    add_test(
        NAME easylocal.rest-execution
        COMMAND easylocal_rest_execution_test
    )

    add_executable(
        easylocal_rest_blueprint_compile_test
        rest_blueprint_compile.cpp
    )
    target_link_libraries(
        easylocal_rest_blueprint_compile_test
        PRIVATE
            EasyLocal::REST
            easylocal_assignment_mwe_model
    )
    add_test(
        NAME easylocal.rest-blueprint-compile
        COMMAND easylocal_rest_blueprint_compile_test
    )

    add_executable(
        easylocal_rest_blueprint_requests_test
        rest_blueprint_requests.cpp
    )
    target_link_libraries(
        easylocal_rest_blueprint_requests_test
        PRIVATE
            EasyLocal::REST
            easylocal_assignment_mwe_model
    )
    add_test(
        NAME easylocal.rest-blueprint-requests
        COMMAND easylocal_rest_blueprint_requests_test
    )
    set_tests_properties(
        easylocal.rest-blueprint-requests
        PROPERTIES
            TIMEOUT 60
    )

    find_program(EASYLOCAL_CURL_EXECUTABLE NAMES curl)
    if(UNIX AND EASYLOCAL_CURL_EXECUTABLE AND TARGET easylocal_assignment_rest_mwe)
        add_test(
            NAME easylocal.rest-http
            COMMAND
                "${CMAKE_CURRENT_SOURCE_DIR}/rest_http_integration.sh"
                "$<TARGET_FILE:easylocal_assignment_rest_mwe>"
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
        if(TARGET easylocal_tsp_tui_mwe)  # the launcher of two apps
            list(APPEND easylocal_tui_e2e_environment
                "EASYLOCAL_TSP_TUI=$<TARGET_FILE:easylocal_tsp_tui_mwe>")
        endif()
        if(TARGET easylocal_assignment_tui_mwe)  # its slow-fi runner can be stopped
            list(APPEND easylocal_tui_e2e_environment
                "EASYLOCAL_ASSIGNMENT_TUI=$<TARGET_FILE:easylocal_assignment_tui_mwe>")
        endif()
        add_test(
            NAME easylocal.tui-e2e
            COMMAND ${easylocal_tui_e2e_python} -m pytest -q -p no:cacheprovider
                "${PROJECT_SOURCE_DIR}/tests/tui"
        )
        set_tests_properties(
            easylocal.tui-e2e
            PROPERTIES
                LABELS "integration;tui-e2e"
                ENVIRONMENT "${easylocal_tui_e2e_environment}"
                TIMEOUT 600
        )
    else()
        message(STATUS "neither uv nor Python with pyte and pytest: TUI end-to-end tests disabled")
    endif()
endif()

add_executable(
    easylocal_trace_test
    trace.cpp
)
target_link_libraries(
    easylocal_trace_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.trace
    COMMAND easylocal_trace_test
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
