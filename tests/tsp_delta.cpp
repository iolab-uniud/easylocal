#include <easylocal/cursor_moves.hpp>
#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

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
    using namespace easylocal::mwe::tsp;

    bool ok = true;

    const TspInstance instance{
        .city_count = 5,
        .distances = {
            0.0, 1.0, 2.0, 2.5, 1.5,
            1.0, 0.0, 1.25, 2.0, 2.5,
            2.0, 1.25, 0.0, 1.0, 2.0,
            2.5, 2.0, 1.0, 0.0, 1.25,
            1.5, 2.5, 2.0, 1.25, 0.0,
        },
    };
    const Tour solution{
        .tour = {0, 2, 1, 3, 4},
    };

    const TspSolutionManager solution_manager{instance};
    const TwoOptNeighborhoodExplorer neighborhood{solution_manager};
    const TourLengthComponent component{instance};
    const TwoOptTourLengthDeltaEvaluator delta_evaluator{instance};

    const auto current = component.evaluate(solution);

    for (const auto move : easylocal::moves(neighborhood, solution))
    {
        Tour candidate = solution;
        neighborhood.make_move(candidate, move);

        const auto full_value = component.evaluate(candidate);
        const auto incremental_value =
            current + delta_evaluator.delta_evaluate(solution, move);

        ok &= expect(
            incremental_value == full_value,
            "2-opt delta law matches full tour-length evaluation");
    }

    const TwoOptMove improving_move{
        .first_edge = 0,
        .second_edge = 2,
    };
    ok &= expect(
        delta_evaluator.delta_evaluate(solution, improving_move) ==
            TourLengthDelta{.change = -2.0},
        "known improving 2-opt move has the expected exact delta");

    return ok ? 0 : 1;
}
