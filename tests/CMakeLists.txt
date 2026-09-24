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

function(easylocal_add_compile_failure_test name source expected_diagnostic)
    add_test(
        NAME "easylocal.compile-fail.${name}"
        COMMAND
            "${CMAKE_COMMAND}"
            "-DCXX=${CMAKE_CXX_COMPILER}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${source}"
            "-DINCLUDE_DIR=${PROJECT_SOURCE_DIR}/include"
            "-DEXPECTED_DIAGNOSTIC=${expected_diagnostic}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_failure.cmake"
    )
endfunction()

easylocal_add_compile_failure_test(
    duplicate-component
    duplicate_component.cpp
    "a SolutionManager recipe may contain each component type at most once"
)
easylocal_add_compile_failure_test(
    duplicate-delta
    duplicate_delta.cpp
    "a neighborhood recipe may attach at most one delta evaluator to each component type"
)
easylocal_add_compile_failure_test(
    inactive-delta
    inactive_delta.cpp
    "every attached delta must name a component that is active in the bound SolutionManager recipe"
)
easylocal_add_compile_failure_test(
    incompatible-delta
    incompatible_delta.cpp
    "every attached delta evaluator must be compatible with the bound component value, Solution, and Move types"
)
easylocal_add_compile_failure_test(
    missing-delta-evaluate
    missing_delta_evaluate.cpp
    "every attached delta evaluator must be compatible with the bound component value, Solution, and Move types"
)
easylocal_add_compile_failure_test(
    delta-not-constructible
    delta_not_constructible.cpp
    "a delta evaluator must be constructible from the bound Instance followed by its recipe arguments"
)
easylocal_add_compile_failure_test(
    component-not-constructible
    component_not_constructible.cpp
    "a cost component must be constructible from the bound Instance followed by its recipe arguments"
)
