#include "move.hpp"
#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"

#include <cstddef>
#include <iostream>
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

    const Instance instance{
        .city_count = 5,
        .distances = std::vector<distance_type>(25, 0.0),
    };
    const SolutionManager solution_manager{instance};
    const NeighborhoodExplorer neighborhood{solution_manager};
    const Solution solution{
        .tour = {0, 2, 1, 3, 4},
    };

    auto all_moves = neighborhood.moves(solution);
    static_assert(std::ranges::input_range<decltype(all_moves)>);
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

    for (const auto move : neighborhood.moves(solution))
    {
        ok &= expect(
            neighborhood.is_valid(solution, move),
            "every generated 2-opt move is structurally valid");
    }

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

    Solution moved = solution;
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

    const Instance triangle_instance{
        .city_count = 3,
        .distances = std::vector<distance_type>(9, 0.0),
    };
    const SolutionManager triangle_manager{triangle_instance};
    const NeighborhoodExplorer triangle_neighborhood{triangle_manager};
    const Solution triangle{
        .tour = {0, 1, 2},
    };

    ok &= expect(
        std::ranges::empty(triangle_neighborhood.moves(triangle)),
        "three-city tour has no non-degenerate 2-opt move");

    return ok ? 0 : 1;
}
