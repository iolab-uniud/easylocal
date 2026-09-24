#include "capacity_delta.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/runner.hpp>

#include <iostream>

int main()
{
    using namespace easylocal::mwe::assignment;
    using easylocal::component;
    using easylocal::delta;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    auto solution_manager =
        (easylocal::solution_manager<SolutionManager>()
         | component<CapacityCostComponent>())
            .construct(instance);

    auto neighborhood_explorer =
        (easylocal::neighborhood<NeighborhoodExplorer>()
         | delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>())
            .construct(solution_manager);

    Solution solution{
        .assignment = {0, 0, 1},
    };

    if (!solution_manager.is_valid(solution))
    {
        return 1;
    }

    std::cout << "initial overload: "
              << solution_manager.evaluate(solution).get<0>()
              << '\n';

    const Move move{
        .job = 1,
        .destination = 1,
    };

    if (!neighborhood_explorer.is_valid(solution, move))
    {
        return 1;
    }

    neighborhood_explorer.make_move(solution, move);

    std::cout << "final overload: "
              << solution_manager.evaluate(solution).get<0>()
              << '\n';

    return 0;
}
