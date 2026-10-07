#include "solution_manager.hpp"
#include "support/expect.hpp"
#include "swap_move.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_component.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <cmath>
#include <cstddef>
#include <map>
#include <random>
#include <ranges>
#include <utility>
#include <vector>

namespace
{

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
    using namespace tsp;

    bool ok = true;

    // Distinct symmetric distances, small integers that doubles add exactly:
    // a wrong edge in the delta changes its value.
    const std::vector<std::vector<distance_type>> rows{
        {0.0, 3.0, 7.0, 2.0, 11.0},
        {3.0, 0.0, 4.0, 9.0, 6.0},
        {7.0, 4.0, 0.0, 5.0, 1.0},
        {2.0, 9.0, 5.0, 0.0, 8.0},
        {11.0, 6.0, 1.0, 8.0, 0.0},
    };
    TspInstance instance{.city_count = rows.size(), .distances = {}};
    for (const auto& row : rows)
        instance.distances.insert(instance.distances.end(), row.begin(), row.end());
    const TspSolutionManager solution_manager{instance};
    const SwapCitiesNeighborhoodExplorer neighborhood{solution_manager};
    const TourLengthComponent tour_length{instance};
    const SwapTourLengthDelta delta_evaluator{instance};
    const Tour solution{
        .tour = {0, 2, 1, 4, 3},
    };

    auto all_moves = easylocal::moves(neighborhood, solution);
    static_assert(std::ranges::input_range<decltype(all_moves)>);
    static_assert(!std::ranges::forward_range<decltype(all_moves)>);
    static_assert(std::ranges::view<decltype(all_moves)>);

    const std::vector<observed_move> expected{
        {0, 1},
        {0, 2},
        {0, 3},
        {0, 4},
        {1, 2},
        {1, 3},
        {1, 4},
        {2, 3},
        {2, 4},
        {3, 4},
    };

    ok &= expect(
        collect(all_moves) == expected,
        "swap moves are generated lazily in lexicographic position order");

    const auto current_length = tour_length.evaluate(solution);
    bool some_delta_nonzero = false;
    for (const auto& [first, second] : expected)
    {
        const SwapCitiesMove move{
            .first_position = first,
            .second_position = second,
        };
        auto candidate = solution;
        neighborhood.make_move(candidate, move);

        const auto delta = delta_evaluator.delta_evaluate(solution, move);
        some_delta_nonzero |= delta != 0.0;
        ok &= expect(
            current_length + delta == tour_length.evaluate(candidate),
            "swap tour-length delta agrees with full evaluation for every move");
    }
    ok &= expect(some_delta_nonzero, "some swap changes the tour length");

    std::mt19937 rng{12345U};
    const auto random_move = neighborhood.random_move(solution, rng);
    ok &= expect(
        random_move.has_value() && neighborhood.is_valid(solution, *random_move),
        "single random swap proposal is available and valid");

    // The random proposal is uniform over the valid moves: every move of the
    // deterministic enumeration, and no other, within 5% of its share.
    {
        std::map<std::pair<std::size_t, std::size_t>, std::size_t> drawn;
        std::size_t valid_count = 0;
        for (const auto move : easylocal::moves(neighborhood, solution))
        {
            drawn[{move.first_position, move.second_position}] = 0;
            ++valid_count;
        }
        constexpr std::size_t draws = 200'000;
        std::mt19937 uniform_rng{2026U};
        bool only_valid = true;
        for (std::size_t draw = 0; draw < draws; ++draw)
        {
            const auto move = neighborhood.random_move(solution, uniform_rng);
            const auto found = drawn.find({move->first_position, move->second_position});
            if (found == drawn.end())
            {
                only_valid = false;
                continue;
            }
            ++found->second;
        }
        const double expected = static_cast<double>(draws) / static_cast<double>(valid_count);
        bool uniform = true;
        for (const auto& [move, count] : drawn)
        {
            uniform &= std::abs(static_cast<double>(count) - expected) <= 0.05 * expected;
        }
        ok &= expect(only_valid, "random swap proposals are moves of the neighborhood");
        ok &= expect(uniform, "random swap proposals are uniform over the neighborhood");
    }

    Tour moved = solution;
    neighborhood.make_move(
        moved,
        SwapCitiesMove{
            .first_position = 1,
            .second_position = 2,
        });
    ok &= expect(
        moved.tour == std::vector<city_id>{0, 1, 2, 4, 3},
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
                .first_position = 4,
                .second_position = 5,
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

    auto singleton_moves = easylocal::moves(singleton_neighborhood, singleton);
    ok &= expect(
        singleton_moves.begin() == singleton_moves.end(),
        "single-city tour has no swap move");

    std::mt19937 singleton_rng{7U};
    ok &= expect(
        !singleton_neighborhood.random_move(singleton, singleton_rng).has_value(),
        "single random swap proposal is empty when no move exists");

    return ok ? 0 : 1;
}
