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

add_executable(
    easylocal_parameters_header_self_containment
    parameters_header_self_containment.cpp
)
target_link_libraries(
    easylocal_parameters_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.parameters-header-self-containment
    COMMAND easylocal_parameters_header_self_containment
)

add_executable(
    easylocal_configuration_tree_header_self_containment
    configuration_tree_header_self_containment.cpp
)
target_link_libraries(
    easylocal_configuration_tree_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-tree-header-self-containment
    COMMAND easylocal_configuration_tree_header_self_containment
)


add_executable(
    easylocal_configuration_overrides_header_self_containment
    configuration_overrides_header_self_containment.cpp
)
target_link_libraries(
    easylocal_configuration_overrides_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-overrides-header-self-containment
    COMMAND easylocal_configuration_overrides_header_self_containment
)

add_executable(
    easylocal_configuration_file_header_self_containment
    configuration_file_header_self_containment.cpp
)
target_link_libraries(
    easylocal_configuration_file_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-file-header-self-containment
    COMMAND easylocal_configuration_file_header_self_containment
)

add_executable(
    easylocal_configuration_cli_header_self_containment
    configuration_cli_header_self_containment.cpp
)
target_link_libraries(
    easylocal_configuration_cli_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-cli-header-self-containment
    COMMAND easylocal_configuration_cli_header_self_containment
)

add_executable(
    easylocal_configuration_setup_header_self_containment
    configuration_setup_header_self_containment.cpp
)
target_link_libraries(
    easylocal_configuration_setup_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.configuration-setup-header-self-containment
    COMMAND easylocal_configuration_setup_header_self_containment
)


if(TARGET EasyLocal::ConfigTOML)
    add_executable(
        easylocal_configuration_toml_header_self_containment
        configuration_toml_header_self_containment.cpp
    )
    target_link_libraries(
        easylocal_configuration_toml_header_self_containment
        PRIVATE
            EasyLocal::ConfigTOML
    )
    add_test(
        NAME easylocal.configuration-toml-header-self-containment
        COMMAND easylocal_configuration_toml_header_self_containment
    )

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
    easylocal_header_self_containment
    header_self_containment.cpp
)
target_link_libraries(
    easylocal_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.header-self-containment
    COMMAND easylocal_header_self_containment
)

add_executable(
    easylocal_service_base_header_self_containment
    service_base_header_self_containment.cpp
)
target_link_libraries(
    easylocal_service_base_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.service-base-header-self-containment
    COMMAND easylocal_service_base_header_self_containment
)

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
    easylocal_runner_tag_header_self_containment
    runner_tag_header_self_containment.cpp
)
target_link_libraries(
    easylocal_runner_tag_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.runner-tag-header-self-containment
    COMMAND easylocal_runner_tag_header_self_containment
)

add_executable(
    easylocal_runner_header_self_containment
    runner_header_self_containment.cpp
)
target_link_libraries(
    easylocal_runner_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.runner-header-self-containment
    COMMAND easylocal_runner_header_self_containment
)

add_executable(
    easylocal_neighborhood_concepts_header_self_containment
    neighborhood_concepts_header_self_containment.cpp
)
target_link_libraries(
    easylocal_neighborhood_concepts_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.neighborhood-concepts-header-self-containment
    COMMAND easylocal_neighborhood_concepts_header_self_containment
)

add_executable(
    easylocal_neighborhood_testing_header_self_containment
    neighborhood_testing_header_self_containment.cpp
)
target_link_libraries(
    easylocal_neighborhood_testing_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.neighborhood-testing-header-self-containment
    COMMAND easylocal_neighborhood_testing_header_self_containment
)

add_executable(
    easylocal_testing_check_header_self_containment
    testing_check_header_self_containment.cpp
)
target_link_libraries(
    easylocal_testing_check_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.testing-check-header-self-containment
    COMMAND easylocal_testing_check_header_self_containment
)

add_executable(
    easylocal_testing_solution_manager_header_self_containment
    testing_solution_manager_header_self_containment.cpp
)
target_link_libraries(
    easylocal_testing_solution_manager_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.testing-solution-manager-header-self-containment
    COMMAND easylocal_testing_solution_manager_header_self_containment
)

add_executable(
    easylocal_testing_cost_component_header_self_containment
    testing_cost_component_header_self_containment.cpp
)
target_link_libraries(
    easylocal_testing_cost_component_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.testing-cost-component-header-self-containment
    COMMAND easylocal_testing_cost_component_header_self_containment
)

add_executable(
    easylocal_testing_delta_evaluator_header_self_containment
    testing_delta_evaluator_header_self_containment.cpp
)
target_link_libraries(
    easylocal_testing_delta_evaluator_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.testing-delta-evaluator-header-self-containment
    COMMAND easylocal_testing_delta_evaluator_header_self_containment
)

add_executable(
    easylocal_testing_checks_header_self_containment
    testing_checks_header_self_containment.cpp
)
target_link_libraries(
    easylocal_testing_checks_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.testing-checks-header-self-containment
    COMMAND easylocal_testing_checks_header_self_containment
)

add_executable(
    easylocal_cursor_moves_header_self_containment
    cursor_moves_header_self_containment.cpp
)
target_link_libraries(
    easylocal_cursor_moves_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.cursor-moves-header-self-containment
    COMMAND easylocal_cursor_moves_header_self_containment
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
    easylocal_cost_header_self_containment
    cost_header_self_containment.cpp
)
target_link_libraries(
    easylocal_cost_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.cost-header-self-containment
    COMMAND easylocal_cost_header_self_containment
)

add_executable(
    easylocal_aggregation_header_self_containment
    aggregation_header_self_containment.cpp
)
target_link_libraries(
    easylocal_aggregation_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.aggregation-header-self-containment
    COMMAND easylocal_aggregation_header_self_containment
)

add_executable(
    easylocal_first_improvement_header_self_containment
    first_improvement_header_self_containment.cpp
)
target_link_libraries(
    easylocal_first_improvement_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.first-improvement-header-self-containment
    COMMAND easylocal_first_improvement_header_self_containment
)

add_executable(
    easylocal_best_improvement_header_self_containment
    best_improvement_header_self_containment.cpp
)
target_link_libraries(
    easylocal_best_improvement_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.best-improvement-header-self-containment
    COMMAND easylocal_best_improvement_header_self_containment
)

add_executable(
    easylocal_metropolis_acceptance_header_self_containment
    metropolis_acceptance_header_self_containment.cpp
)
target_link_libraries(
    easylocal_metropolis_acceptance_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.metropolis-acceptance-header-self-containment
    COMMAND easylocal_metropolis_acceptance_header_self_containment
)

add_executable(
    easylocal_temperature_policy_header_self_containment
    temperature_policy_header_self_containment.cpp
)
target_link_libraries(
    easylocal_temperature_policy_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.temperature-policy-header-self-containment
    COMMAND easylocal_temperature_policy_header_self_containment
)

add_executable(
    easylocal_simulated_annealing_header_self_containment
    simulated_annealing_header_self_containment.cpp
)
target_link_libraries(
    easylocal_simulated_annealing_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.simulated-annealing-header-self-containment
    COMMAND easylocal_simulated_annealing_header_self_containment
)

add_executable(
    easylocal_aggregation_test
    aggregation.cpp
)
target_link_libraries(
    easylocal_aggregation_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.aggregation
    COMMAND easylocal_aggregation_test
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
    easylocal_solution_manager_concepts_header_self_containment
    solution_manager_concepts_header_self_containment.cpp
)
target_link_libraries(
    easylocal_solution_manager_concepts_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.solution-manager-concepts-header-self-containment
    COMMAND easylocal_solution_manager_concepts_header_self_containment
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
    easylocal_configuration_tree_test
    configuration_tree.cpp
)
target_link_libraries(
    easylocal_configuration_tree_test
    PRIVATE
        easylocal_tsp_mwe_model
)
add_test(
    NAME easylocal.configuration-tree
    COMMAND easylocal_configuration_tree_test
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
    easylocal_neighborhood_union_header_self_containment
    neighborhood_union_header_self_containment.cpp
)
target_link_libraries(
    easylocal_neighborhood_union_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.neighborhood-union-header-self-containment
    COMMAND easylocal_neighborhood_union_header_self_containment
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
        easylocal_tui_header_self_containment
        tui_header_self_containment.cpp
    )
    target_link_libraries(
        easylocal_tui_header_self_containment
        PRIVATE
            EasyLocal::TUI
    )
    add_test(
        NAME easylocal.tui-header-self-containment
        COMMAND easylocal_tui_header_self_containment
    )

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

add_test(
    NAME easylocal.installed-package-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DEASYLOCAL_CONFIG_TOML_ENABLED=${_easylocal_config_toml_enabled}"
        "-DEASYLOCAL_CONFIG_TOML_BUNDLED=${_easylocal_config_toml_bundled}"
        "-DEASYLOCAL_TUI_ENABLED=${_easylocal_tui_enabled}"
        "-DEASYLOCAL_TUI_BUNDLED=${_easylocal_tui_bundled}"
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
    tester-missing-solution-factory
    tester_missing_solution_factory.cpp
    "Tester requires the SolutionManager to provide initial_solution() or random_solution(std::mt19937_64&)"
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
    missing-aggregator
    missing_aggregator.cpp
    "a SolutionManager recipe with cost components requires an explicit aggregator"
    "NonAggregatableComponent"
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
    duplicate-configuration-node-name
    duplicate_configuration_node_name.cpp
    "configuration siblings must have unique names"
    "duplicate"
)
easylocal_add_compile_failure_test(
    duplicate-parameter-field-name
    duplicate_parameter_field_name.cpp
    "parameter fields must have unique names within a block"
    "duplicate"
)

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

add_executable(easylocal_solver_header_self_containment solver_header_self_containment.cpp)
target_link_libraries(easylocal_solver_header_self_containment PRIVATE EasyLocal::Core)
add_test(NAME easylocal.solver-header-self-containment COMMAND easylocal_solver_header_self_containment)

add_executable(easylocal_solver_test solver.cpp)
target_link_libraries(easylocal_solver_test PRIVATE EasyLocal::Core)
add_test(NAME easylocal.solver COMMAND easylocal_solver_test)

add_executable(easylocal_multi_start_solver_test multi_start_solver.cpp)
target_link_libraries(easylocal_multi_start_solver_test PRIVATE EasyLocal::Core)
add_test(NAME easylocal.multi-start-solver COMMAND easylocal_multi_start_solver_test)

add_executable(easylocal_two_stage_solver_test two_stage_solver.cpp)
target_link_libraries(easylocal_two_stage_solver_test PRIVATE EasyLocal::Core)
add_test(NAME easylocal.two-stage-solver COMMAND easylocal_two_stage_solver_test)

add_executable(
    easylocal_app_header_self_containment
    app_header_self_containment.cpp
)
target_link_libraries(
    easylocal_app_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.app-header-self-containment
    COMMAND easylocal_app_header_self_containment
)

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
    easylocal_check_header_self_containment
    check_header_self_containment.cpp
)
target_link_libraries(
    easylocal_check_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.check-header-self-containment
    COMMAND easylocal_check_header_self_containment
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
    easylocal_tester_header_self_containment
    tester_header_self_containment.cpp
)
target_link_libraries(
    easylocal_tester_header_self_containment
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.tester-header-self-containment
    COMMAND easylocal_tester_header_self_containment
)

add_executable(
    easylocal_tester_test
    tester.cpp
)
target_link_libraries(
    easylocal_tester_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.tester
    COMMAND easylocal_tester_test
)

add_executable(
    easylocal_tester_io_test
    tester_io.cpp
)
target_link_libraries(
    easylocal_tester_io_test
    PRIVATE
        EasyLocal::Core
)
add_test(
    NAME easylocal.tester-io
    COMMAND easylocal_tester_io_test
)
