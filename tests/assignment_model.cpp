#include "capacity_delta.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/core/aggregation.hpp>
#include <easylocal/runners/runner.hpp>

#include <compare>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace easylocal::mwe::assignment;

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    using namespace easylocal::mwe::assignment;
    using easylocal::aggregator;
    using easylocal::component;
    using easylocal::solution_manager;

    static_assert(std::three_way_comparable<HardCost>);
    static_assert(std::three_way_comparable<Cost>);

    bool ok = true;

    const AssignmentInstance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const auto manager_recipe =
        solution_manager<AssignmentSolutionManager>()
        | component<CapacityCostComponent>()
        | aggregator([](const CapacityValue& capacity) {
              return AssignmentCostAggregator{}.hard(capacity);
          });
    const auto configured_solution_manager = manager_recipe.construct(instance);

    const AssignmentSolution initial{
        .assignment = {0, 0, 1},
    };

    // Structural validity and feasibility are distinct: this solution is valid
    // even though it has positive overload.
    ok &= expect(
        configured_solution_manager.is_valid(initial),
        "overloaded solution is structurally valid");

    const auto generated_initial = configured_solution_manager.initial_solution();
    ok &= expect(
        generated_initial.assignment == std::vector<machine_id>{0, 1, 0},
        "deterministic initial solution uses round-robin assignment");

    const auto initial_cost = configured_solution_manager.evaluate(initial);

    ok &= expect(
        initial_cost == HardCost{2, 1},
        "structured cost keeps total overload primary and overloaded-machine count secondary");
    ok &= expect(
        initial_cost.get<0>() == 2 &&
            initial_cost.get<1>() == 1,
        "lexicographic hard cost keeps both materialized components");

    ok &= expect(
        HardCost{0, 100} < HardCost{1, 0},
        "lexicographic hard cost prioritizes total overload");
    ok &= expect(
        HardCost{1, 1} < HardCost{1, 2},
        "lexicographic hard cost compares overloaded-machine count after a tie");

    // Adding the real soft component upgrades the cost from the hard
    // lexicographic branch to the full hierarchical hard/soft model.
    const auto full_recipe =
        solution_manager<AssignmentSolutionManager>()
        | component<CapacityCostComponent>()
        | component<LoadImbalanceCostComponent>()
        | aggregator(AssignmentCostAggregator{});

    const auto full_manager = full_recipe.construct(instance);
    const auto full_cost = full_manager.evaluate(initial);

    ok &= expect(
        full_cost.hard() == HardCost{2, 1} && full_cost.soft() == 5,
        "full assignment cost composes hard feasibility and soft load balance");

    // A delta is a separate, materialized value. Applying it to the current
    // component value must match full evaluation after the move.
    const CapacityCostComponent capacity_component{instance};
    const ReassignCapacityDeltaEvaluator capacity_delta{instance};
    const AssignmentSolutionManager neighborhood_manager{instance};
    const ReassignJobNeighborhoodExplorer neighborhood{neighborhood_manager};

    const auto before = capacity_component.evaluate(initial);

    for (const auto move : easylocal::moves(neighborhood, initial))
    {
        AssignmentSolution candidate = initial;
        neighborhood.make_move(candidate, move);

        const auto delta = capacity_delta.delta_evaluate(initial, move);
        const auto incrementally_updated = before + delta;
        const auto fully_evaluated = capacity_component.evaluate(candidate);

        ok &= expect(
            incrementally_updated == fully_evaluated,
            "structured delta agrees with full component evaluation");
    }

    const ReassignJobMove relieving_move{
        .job = 1,
        .destination = 1,
    };

    ok &= expect(
        capacity_delta.delta_evaluate(initial, relieving_move) ==
            CapacityDelta{-1, -2},
        "structured delta materializes both changed capacity fields");

    // AssignmentSolution has ordinary value semantics.
    AssignmentSolution copy = initial;
    copy.assignment[0] = 1;

    ok &= expect(
        initial.assignment[0] == 0,
        "solution copy is independent");

    // Invalid solution representations are detected by the manager.
    const AssignmentSolution wrong_size{
        .assignment = {0, 1},
    };

    ok &= expect(
        !configured_solution_manager.is_valid(wrong_size),
        "wrong assignment cardinality is invalid");

    const AssignmentSolution bad_machine{
        .assignment = {0, 2, 1},
    };

    ok &= expect(
        !configured_solution_manager.is_valid(bad_machine),
        "out-of-range machine id is invalid");

    // Two instance-bound managers can coexist in one process.
    const AssignmentInstance roomy_instance{
        .demand = {4, 3, 2},
        .capacity = {10, 10},
    };

    const auto roomy_manager = manager_recipe.construct(roomy_instance);

    ok &= expect(
        roomy_manager.is_valid(initial),
        "same representation is valid for a second instance");

    ok &= expect(
        roomy_manager.evaluate(initial) == HardCost{0, 0},
        "same solution is evaluated relative to the manager's instance");

    ok &= expect(
        configured_solution_manager.evaluate(initial) == HardCost{2, 1},
        "first manager remains bound to the first instance");

    return ok ? 0 : 1;
}
