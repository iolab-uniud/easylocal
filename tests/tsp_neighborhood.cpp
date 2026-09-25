#include "move.hpp"
#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"

#include <concepts>
#include <cstddef>
#include <iostream>
#include <random>
#include <ranges>
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
        result.emplace_back(move.first_edge, move.second_edge);
    }

    return result;
}

} // namespace

int main()
{
    using namespace easylocal::mwe::tsp;

    bool ok = true;

    const TspInstance instance{
        .city_count = 5,
        .distances = std::vector<distance_type>(25, 0.0),
    };
    const TspSolutionManager solution_manager{instance};
    const TwoOptNeighborhoodExplorer neighborhood{solution_manager};
    const Tour solution{
        .tour = {0, 2, 1, 3, 4},
    };

    auto all_moves = neighborhood.moves(solution);
    static_assert(std::ranges::input_range<decltype(all_moves)>);
    static_assert(!std::ranges::forward_range<decltype(all_moves)>);
    static_assert(std::ranges::view<decltype(all_moves)>);

    const std::vector<observed_move> expected{
        {0, 2},
        {0, 3},
        {1, 3},
        {1, 4},
        {2, 4},
    };

    ok &= expect(
        collect(all_moves) == expected,
        "2-opt moves are generated lazily in lexicographic edge order");

    std::vector<observed_move> cursor_observed;
    TwoOptMove cursor_move{};
    if (neighborhood.first_move(solution, cursor_move))
    {
        do
        {
            cursor_observed.emplace_back(
                cursor_move.first_edge,
                cursor_move.second_edge);
        } while (neighborhood.next_move(solution, cursor_move));
    }

    ok &= expect(
        cursor_observed == expected,
        "FirstMove/NextMove authoring protocol matches the 2-opt move range");

    for (const auto move : neighborhood.moves(solution))
    {
        ok &= expect(
            neighborhood.is_valid(solution, move),
            "every generated 2-opt move is structurally valid");
    }

    static_assert(std::same_as<
        TwoOptNeighborhoodExplorer::random_sampling,
        easylocal::sampling::with_replacement>);

    std::mt19937 rng{12345U};
    const auto random_move = neighborhood.random_move(solution, rng);
    ok &= expect(
        random_move.has_value() && neighborhood.is_valid(solution, *random_move),
        "single random 2-opt proposal is available and valid");

    auto random_moves = neighborhood.random_moves(solution, rng);
    static_assert(std::ranges::input_range<decltype(random_moves)>);
    static_assert(!std::ranges::forward_range<decltype(random_moves)>);
    static_assert(std::ranges::view<decltype(random_moves)>);

    ok &= expect(
        !std::ranges::empty(random_moves),
        "random 2-opt traversal is non-empty when moves exist");

    std::size_t random_samples = 0;
    for (const auto move : random_moves | std::views::take(32))
    {
        ++random_samples;
        ok &= expect(
            neighborhood.is_valid(solution, move),
            "with-replacement traversal yields only valid 2-opt moves");
    }

    ok &= expect(
        random_samples == 32,
        "with-replacement traversal remains available beyond neighborhood size");

    auto first_edge_zero =
        neighborhood.moves(solution)
        | std::views::filter([](const TwoOptMove move) {
              return move.first_edge == 0;
          });

    const std::vector<observed_move> expected_filtered{
        {0, 2},
        {0, 3},
    };

    ok &= expect(
        collect(first_edge_zero) == expected_filtered,
        "2-opt move range composes with standard views");

    Tour moved = solution;
    const TwoOptMove improving_move{
        .first_edge = 0,
        .second_edge = 2,
    };

    neighborhood.make_move(moved, improving_move);

    ok &= expect(
        moved.tour == std::vector<city_id>{0, 1, 2, 3, 4},
        "2-opt reverses exactly the segment between the cut edges");

    ok &= expect(
        !neighborhood.is_valid(
            solution,
            TwoOptMove{.first_edge = 0, .second_edge = 1}),
        "adjacent edges do not define a 2-opt move");
    ok &= expect(
        !neighborhood.is_valid(
            solution,
            TwoOptMove{.first_edge = 0, .second_edge = 4}),
        "first and last tour edges are adjacent through wraparound");
    ok &= expect(
        !neighborhood.is_valid(
            solution,
            TwoOptMove{.first_edge = 2, .second_edge = 0}),
        "2-opt edge pairs use a canonical increasing order");
    ok &= expect(
        !neighborhood.is_valid(
            solution,
            TwoOptMove{.first_edge = 1, .second_edge = 5}),
        "out-of-range edge index is invalid");

    const TspInstance triangle_instance{
        .city_count = 3,
        .distances = std::vector<distance_type>(9, 0.0),
    };
    const TspSolutionManager triangle_manager{triangle_instance};
    const TwoOptNeighborhoodExplorer triangle_neighborhood{triangle_manager};
    const Tour triangle{
        .tour = {0, 1, 2},
    };

    auto triangle_moves = triangle_neighborhood.moves(triangle);
    ok &= expect(
        triangle_moves.begin() == triangle_moves.end(),
        "three-city tour has no non-degenerate 2-opt move");

    std::mt19937 triangle_rng{7U};
    ok &= expect(
        !triangle_neighborhood.random_move(triangle, triangle_rng).has_value(),
        "single random 2-opt proposal is empty when no move exists");
    auto triangle_random_moves =
        triangle_neighborhood.random_moves(triangle, triangle_rng);
    ok &= expect(
        std::ranges::empty(triangle_random_moves),
        "random 2-opt traversal is empty when no move exists");

    return ok ? 0 : 1;
}
