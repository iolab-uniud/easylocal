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
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const auto manager_recipe =
        solution_manager<SolutionManager>()
        | component<CapacityCostComponent>();
    const auto neighborhood_recipe =
        neighborhood<NeighborhoodExplorer>()
        | delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();

    auto configured_manager = manager_recipe.construct(instance);
    auto configured_neighborhood =
        neighborhood_recipe.construct(configured_manager);

    Solution solution{
        .assignment = {0, 0, 1},
    };

    if (!configured_manager.is_valid(solution))
    {
        return 1;
    }

    std::cout << "initial overload: "
              << configured_manager.evaluate(solution).get<0>()
              << '\n';

    const Move move{
        .job = 1,
        .destination = 1,
    };

    if (!configured_neighborhood.is_valid(solution, move))
    {
        return 1;
    }

    configured_neighborhood.make_move(solution, move);

    std::cout << "final overload: "
              << configured_manager.evaluate(solution).get<0>()
              << '\n';

    return 0;
}
