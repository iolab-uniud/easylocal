#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <iostream>

int main()
{
    using namespace easylocal::mwe::assignment;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const SolutionManager solution_manager{instance};
    const NeighborhoodExplorer neighborhood{solution_manager};

    Solution solution{
        .assignment = {0, 0, 1},
    };

    if (!solution_manager.is_valid(solution))
    {
        return 1;
    }

    std::cout << "initial overload: "
              << solution_manager.evaluate(solution).get<1>()
              << '\n';

    const Move move{
        .job = 1,
        .destination = 1,
    };

    if (!neighborhood.is_valid(solution, move))
    {
        return 1;
    }

    neighborhood.make_move(solution, move);

    std::cout << "final overload: "
              << solution_manager.evaluate(solution).get<1>()
              << '\n';

    return 0;
}
