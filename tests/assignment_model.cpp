#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{

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

    bool ok = true;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const SolutionManager solution_manager{instance};
    const NeighborhoodExplorer neighborhood{solution_manager};

    const Solution initial{
        .assignment = {0, 0, 1},
    };

    // Structural validity and feasibility are distinct: this solution is valid
    // even though it has positive overload.
    ok &= expect(
        solution_manager.is_valid(initial),
        "overloaded solution is structurally valid");

    ok &= expect(
        solution_manager.evaluate(initial) == 2,
        "hand-computed initial overload is 2");

    // Solution has ordinary value semantics.
    Solution copy = initial;
    copy.assignment[0] = 1;

    ok &= expect(
        initial.assignment[0] == 0,
        "solution copy is independent");

    // Move is a descriptive value; application belongs to the explorer.
    Solution moved = initial;

    const Move move{
        .job = 1,
        .destination = 1,
    };

    ok &= expect(
        neighborhood.is_valid(moved, move),
        "move is valid");

    neighborhood.apply(moved, move);

    ok &= expect(
        moved.assignment == std::vector<machine_id>{0, 1, 1},
        "move changes only the requested assignment");

    ok &= expect(
        solution_manager.evaluate(moved) == 0,
        "move removes the overload");

    // Invalid solution representations are detected by the manager.
    const Solution wrong_size{
        .assignment = {0, 1},
    };

    ok &= expect(
        !solution_manager.is_valid(wrong_size),
        "wrong assignment cardinality is invalid");

    const Solution bad_machine{
        .assignment = {0, 2, 1},
    };

    ok &= expect(
        !solution_manager.is_valid(bad_machine),
        "out-of-range machine id is invalid");

    // Invalid/no-op moves are detected by the explorer.
    ok &= expect(
        !neighborhood.is_valid(
            initial,
            Move{.job = 3, .destination = 1}),
        "out-of-range job is invalid");

    ok &= expect(
        !neighborhood.is_valid(
            initial,
            Move{.job = 0, .destination = 2}),
        "out-of-range destination is invalid");

    ok &= expect(
        !neighborhood.is_valid(
            initial,
            Move{.job = 0, .destination = 0}),
        "no-op move is invalid");

    // Two instance-bound manager graphs can coexist in one process.
    const Instance roomy_instance{
        .demand = {4, 3, 2},
        .capacity = {10, 10},
    };

    const SolutionManager roomy_manager{roomy_instance};
    const NeighborhoodExplorer roomy_neighborhood{roomy_manager};

    ok &= expect(
        roomy_manager.is_valid(initial),
        "same representation is valid for a second instance");

    ok &= expect(
        roomy_manager.evaluate(initial) == 0,
        "same solution is evaluated relative to the manager's instance");

    ok &= expect(
        solution_manager.evaluate(initial) == 2,
        "first manager remains bound to the first instance");

    ok &= expect(
        roomy_neighborhood.is_valid(initial, move),
        "second instance has an independent neighborhood service");

    return ok ? 0 : 1;
}
