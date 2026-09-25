add_executable(
    easylocal_toolchain_smoke
    toolchain_smoke.cpp
)
target_link_libraries(
    easylocal_toolchain_smoke
    PRIVATE
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.configuration-tree-header-self-containment
    COMMAND easylocal_configuration_tree_header_self_containment
)

add_executable(
    easylocal_header_self_containment
    header_self_containment.cpp
)
target_link_libraries(
    easylocal_header_self_containment
    PRIVATE
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.service-base-header-self-containment
    COMMAND easylocal_service_base_header_self_containment
)

add_executable(
    easylocal_runner_header_self_containment
    runner_header_self_containment.cpp
)
target_link_libraries(
    easylocal_runner_header_self_containment
    PRIVATE
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.runner-header-self-containment
    COMMAND easylocal_runner_header_self_containment
)

add_executable(
    easylocal_cursor_moves_header_self_containment
    cursor_moves_header_self_containment.cpp
)
target_link_libraries(
    easylocal_cursor_moves_header_self_containment
    PRIVATE
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.multi-tu
    COMMAND easylocal_multi_tu
)

add_executable(
    easylocal_aggregation_header_self_containment
    aggregation_header_self_containment.cpp
)
target_link_libraries(
    easylocal_aggregation_header_self_containment
    PRIVATE
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.service-base
    COMMAND easylocal_service_base_test
)

add_executable(
    easylocal_concept_contracts_test
    concept_contracts.cpp
)
target_link_libraries(
    easylocal_concept_contracts_test
    PRIVATE
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.concept-contracts
    COMMAND easylocal_concept_contracts_test
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
        EasyLocal::EasyLocal
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
        EasyLocal::EasyLocal
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
    easylocal_neighborhood_union_header_self_containment
    neighborhood_union_header_self_containment.cpp
)
target_link_libraries(
    easylocal_neighborhood_union_header_self_containment
    PRIVATE
        EasyLocal::EasyLocal
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

add_test(
    NAME easylocal.installed-package-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEASYLOCAL_BUILD_DIR=${PROJECT_BINARY_DIR}"
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
    "a delta evaluator must be constructible from the bound Instance followed by its recipe arguments"
    "NonConstructibleDeltaA"
)
easylocal_add_compile_failure_test(
    component-not-constructible
    component_not_constructible.cpp
    "a cost component must be constructible from the bound Instance followed by its recipe arguments"
    "NonConstructibleComponent"
)
easylocal_add_compile_failure_test(
    simulated-annealing-non-numeric-cost
    simulated_annealing_non_numeric_cost.cpp
    "SimulatedAnnealing requires a numeric cost_type"
    "StructuredCost"
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
