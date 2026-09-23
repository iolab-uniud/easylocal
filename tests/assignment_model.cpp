#include "solution_manager.hpp"

#include <compare>
#include <iostream>
#include <string_view>

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

    static_assert(std::three_way_comparable<Cost>);

    bool ok = true;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const SolutionManager solution_manager{instance};

    const Solution initial{
        .assignment = {0, 0, 1},
    };

    // Structural validity and feasibility are distinct: this solution is valid
    // even though it has positive overload.
    ok &= expect(
        solution_manager.is_valid(initial),
        "overloaded solution is structurally valid");

    ok &= expect(
        solution_manager.evaluate(initial) == Cost{2},
        "hand-computed initial overload is 2");

    ok &= expect(Cost{1} < Cost{2}, "lower cost compares as better");
    ok &= expect(Cost{2} > Cost{1}, "higher cost compares as worse");
    ok &= expect(Cost{2} <= Cost{2}, "equal costs satisfy non-strict ordering");

    // Solution has ordinary value semantics.
    Solution copy = initial;
    copy.assignment[0] = 1;

    ok &= expect(
        initial.assignment[0] == 0,
        "solution copy is independent");

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

    // Two instance-bound managers can coexist in one process.
    const Instance roomy_instance{
        .demand = {4, 3, 2},
        .capacity = {10, 10},
    };

    const SolutionManager roomy_manager{roomy_instance};

    ok &= expect(
        roomy_manager.is_valid(initial),
        "same representation is valid for a second instance");

    ok &= expect(
        roomy_manager.evaluate(initial) == Cost{0},
        "same solution is evaluated relative to the manager's instance");

    ok &= expect(
        solution_manager.evaluate(initial) == Cost{2},
        "first manager remains bound to the first instance");

    return ok ? 0 : 1;
}
