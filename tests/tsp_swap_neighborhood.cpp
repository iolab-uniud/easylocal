#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_move.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_component.hpp"

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
        result.emplace_back(move.first_position, move.second_position);
    }

    return result;
}

} // namespace

int main()
{
    using namespace easylocal::mwe::tsp;

    bool ok = true;

    const TspInstance instance{
        .city_count = 4,
        .distances = std::vector<distance_type>(16, 0.0),
    };
    const TspSolutionManager solution_manager{instance};
    const SwapCitiesNeighborhoodExplorer neighborhood{solution_manager};
    const TourLengthComponent tour_length{instance};
    const SwapTourLengthDeltaEvaluator delta_evaluator{instance};
    const Tour solution{
        .tour = {0, 2, 1, 3},
    };

    auto all_moves = neighborhood.moves(solution);
    static_assert(std::ranges::input_range<decltype(all_moves)>);
    static_assert(!std::ranges::forward_range<decltype(all_moves)>);
    static_assert(std::ranges::view<decltype(all_moves)>);

    const std::vector<observed_move> expected{
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3},
    };

    ok &= expect(
        collect(all_moves) == expected,
        "swap moves are generated lazily in lexicographic position order");

    const auto current_length = tour_length.evaluate(solution);
    for (const auto [first, second] : expected)
    {
        const SwapCitiesMove move{
            .first_position = first,
            .second_position = second,
        };
        auto candidate = solution;
        neighborhood.make_move(candidate, move);

        ok &= expect(
            current_length + delta_evaluator.delta_evaluate(solution, move) ==
                tour_length.evaluate(candidate),
            "swap tour-length delta agrees with full evaluation for every move");
    }

    std::mt19937 rng{12345U};
    const auto random_move = neighborhood.random_move(solution, rng);
    ok &= expect(
        random_move.has_value() && neighborhood.is_valid(solution, *random_move),
        "single random swap proposal is available and valid");

    Tour moved = solution;
    neighborhood.make_move(
        moved,
        SwapCitiesMove{
            .first_position = 1,
            .second_position = 2,
        });
    ok &= expect(
        moved.tour == std::vector<city_id>{0, 1, 2, 3},
        "swap move exchanges exactly the selected tour positions");

    ok &= expect(
        !neighborhood.is_valid(
            solution,
            SwapCitiesMove{
                .first_position = 2,
                .second_position = 2,
            }),
        "swap move requires distinct positions in canonical order");
    ok &= expect(
        !neighborhood.is_valid(
            solution,
            SwapCitiesMove{
                .first_position = 3,
                .second_position = 4,
            }),
        "out-of-range swap position is invalid");

    const TspInstance singleton_instance{
        .city_count = 1,
        .distances = {0.0},
    };
    const TspSolutionManager singleton_manager{singleton_instance};
    const SwapCitiesNeighborhoodExplorer singleton_neighborhood{
        singleton_manager};
    const Tour singleton{
        .tour = {0},
    };

    auto singleton_moves = singleton_neighborhood.moves(singleton);
    ok &= expect(
        singleton_moves.begin() == singleton_moves.end(),
        "single-city tour has no swap move");

    std::mt19937 singleton_rng{7U};
    ok &= expect(
        !singleton_neighborhood.random_move(singleton, singleton_rng).has_value(),
        "single random swap proposal is empty when no move exists");

    return ok ? 0 : 1;
}
