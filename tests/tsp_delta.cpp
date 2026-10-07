#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"
#include "support/expect.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

int main()
{
    using namespace tsp;

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
    const TwoOptTourLengthDelta length_delta{instance};

    const auto current = component.evaluate(solution);

    for (const auto move : easylocal::moves(neighborhood, solution))
    {
        Tour candidate = solution;
        neighborhood.make_move(candidate, move);

        const auto full_value = component.evaluate(candidate);
        const auto incremental_value =
            current + length_delta.delta_evaluate(solution, move);

        ok &= expect(
            incremental_value == full_value,
            "2-opt delta law matches full tour-length evaluation");
    }

    const TwoOptMove improving_move{
        .first_edge = 0,
        .second_edge = 2,
    };
    ok &= expect(
        length_delta.delta_evaluate(solution, improving_move) == -2.0,
        "known improving 2-opt move has the expected exact delta");

    return ok ? 0 : 1;
}
