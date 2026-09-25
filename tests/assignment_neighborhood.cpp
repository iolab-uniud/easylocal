#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <concepts>
#include <cstddef>
#include <iostream>
#include <random>
#include <ranges>
#include <set>
#include <string_view>
#include <utility>
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

using observed_move = std::pair<std::size_t, std::size_t>;

template<std::ranges::input_range Range>
auto collect(Range&& range) -> std::vector<observed_move>
{
    std::vector<observed_move> result;

    for (const auto move : range)
    {
        result.emplace_back(move.job, move.destination);
    }

    return result;
}

} // namespace

int main()
{
    using namespace easylocal::mwe::assignment;

    bool ok = true;

    const AssignmentInstance instance{
        .demand = {2, 2},
        .capacity = {3, 3, 3},
    };

    const AssignmentSolutionManager solution_manager{instance};
    const ReassignJobNeighborhoodExplorer neighborhood{solution_manager};

    static_assert(std::same_as<
                  ReassignJobNeighborhoodExplorer::random_sampling,
                  easylocal::sampling::without_replacement>);

    const AssignmentSolution solution{
        .assignment = {1, 2},
    };

    auto all_moves = neighborhood.moves(solution);
    static_assert(std::ranges::input_range<decltype(all_moves)>);
    static_assert(!std::ranges::forward_range<decltype(all_moves)>);
    static_assert(std::ranges::view<decltype(all_moves)>);

    const auto observed = collect(all_moves);
    const std::vector<observed_move> expected{
        {0, 0},
        {0, 2},
        {1, 0},
        {1, 1},
    };

    ok &= expect(
        observed == expected,
        "moves are generated deterministically in job-major order");

    std::vector<observed_move> cursor_observed;
    ReassignJobMove cursor_move{};
    if (neighborhood.first_move(solution, cursor_move))
    {
        do
        {
            cursor_observed.emplace_back(
                cursor_move.job,
                cursor_move.destination);
        } while (neighborhood.next_move(solution, cursor_move));
    }

    ok &= expect(
        cursor_observed == expected,
        "FirstMove/NextMove authoring protocol matches the move range");

    for (const auto move : neighborhood.moves(solution))
    {
        ok &= expect(
            neighborhood.is_valid(solution, move),
            "every generated move is structurally valid");
    }

    auto destination_zero =
        neighborhood.moves(solution)
        | std::views::filter([](const ReassignJobMove move) {
              return move.destination == 0;
          });

    const std::vector<observed_move> expected_filtered{
        {0, 0},
        {1, 0},
    };

    ok &= expect(
        collect(destination_zero) == expected_filtered,
        "move range composes with std::views::filter");

    AssignmentSolution moved = solution;
    const ReassignJobMove move{
        .job = 0,
        .destination = 2,
    };

    ok &= expect(
        neighborhood.is_valid(moved, move),
        "explicit move is valid");

    neighborhood.make_move(moved, move);

    ok &= expect(
        moved.assignment == std::vector<machine_id>{2, 2},
        "make_move changes only the requested assignment");

    ok &= expect(
        !neighborhood.is_valid(
            solution,
            ReassignJobMove{.job = 2, .destination = 0}),
        "out-of-range job is invalid");

    ok &= expect(
        !neighborhood.is_valid(
            solution,
            ReassignJobMove{.job = 0, .destination = 3}),
        "out-of-range destination is invalid");

    ok &= expect(
        !neighborhood.is_valid(
            solution,
            ReassignJobMove{.job = 0, .destination = 1}),
        "no-op move is invalid");

    // The assignment explorer declares one random traversal semantics:
    // without replacement. The complete range is therefore finite and visits
    // each move exactly once in random order.
    std::mt19937 rng_without{12345};
    const auto random_move = neighborhood.random_move(solution, rng_without);
    ok &= expect(
        random_move.has_value() && neighborhood.is_valid(solution, *random_move),
        "single random assignment proposal is available and valid");

    auto unique_random = neighborhood.random_moves(solution, rng_without);

    static_assert(std::ranges::input_range<decltype(unique_random)>);
    static_assert(std::ranges::view<decltype(unique_random)>);

    const auto unique_observed = collect(unique_random);
    const std::set<observed_move> unique_set{
        unique_observed.begin(),
        unique_observed.end(),
    };

    ok &= expect(
        unique_observed.size() == expected.size(),
        "without-replacement traversal exhausts the neighborhood");
    ok &= expect(
        unique_set.size() == unique_observed.size(),
        "without-replacement traversal contains no duplicate moves");

    for (const auto& sampled : unique_observed)
    {
        ok &= expect(
            std::ranges::find(expected, sampled) != expected.end(),
            "random traversal samples only neighborhood moves");
    }

    // Sampling budget belongs to the consumer, not to random_moves().
    std::mt19937 rng_budget{67890};
    auto budgeted_random =
        neighborhood.random_moves(solution, rng_budget)
        | std::views::take(2);

    const auto budgeted_observed = collect(budgeted_random);
    const std::set<observed_move> budgeted_set{
        budgeted_observed.begin(),
        budgeted_observed.end(),
    };

    ok &= expect(
        budgeted_observed.size() == 2,
        "views::take imposes the consumer sampling budget");
    ok &= expect(
        budgeted_set.size() == budgeted_observed.size(),
        "budgeted without-replacement sample remains unique");

    // Filtering before take(1) is safe for this explorer because its random
    // traversal is finite. It either finds one matching move or exhausts the
    // neighborhood.
    std::mt19937 rng_filtered{24680};
    auto filtered_random =
        neighborhood.random_moves(solution, rng_filtered)
        | std::views::filter([](const ReassignJobMove move) {
              return move.destination == 0;
          })
        | std::views::take(1);

    ok &= expect(
        collect(filtered_random).size() == 1,
        "finite random traversal supports filter followed by take(1)");

    const AssignmentInstance single_machine_instance{
        .demand = {1, 2},
        .capacity = {10},
    };
    const AssignmentSolutionManager single_machine_manager{single_machine_instance};
    const ReassignJobNeighborhoodExplorer single_machine_neighborhood{
        single_machine_manager};
    const AssignmentSolution single_machine_solution{
        .assignment = {0, 0},
    };

    auto single_machine_moves =
        single_machine_neighborhood.moves(single_machine_solution);
    ok &= expect(
        single_machine_moves.begin() == single_machine_moves.end(),
        "one-machine assignment has an empty move range");

    std::mt19937 empty_rng{1};
    ok &= expect(
        !single_machine_neighborhood.random_move(
            single_machine_solution,
            empty_rng).has_value(),
        "single random assignment proposal is empty when no move exists");
    ok &= expect(
        std::ranges::empty(
            single_machine_neighborhood.random_moves(
                single_machine_solution,
                empty_rng)),
        "random traversal of an empty neighborhood is empty");

    const AssignmentInstance empty_instance{
        .demand = {},
        .capacity = {3, 3, 3},
    };
    const AssignmentSolutionManager empty_manager{empty_instance};
    const ReassignJobNeighborhoodExplorer empty_neighborhood{empty_manager};
    const AssignmentSolution empty_solution{
        .assignment = {},
    };

    auto empty_moves = empty_neighborhood.moves(empty_solution);
    ok &= expect(
        empty_moves.begin() == empty_moves.end(),
        "empty assignment has an empty move range");

    return ok ? 0 : 1;
}
