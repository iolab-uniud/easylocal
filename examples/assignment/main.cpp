#include "capacity_delta.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/runner.hpp>
#include <easylocal/search/first_improvement.hpp>

#include <cstddef>
#include <iostream>

namespace
{

using namespace easylocal::mwe::assignment;

void print_solution(const AssignmentSolution& solution)
{
    std::cout << '[';

    for (std::size_t job = 0; job < solution.assignment.size(); ++job)
    {
        if (job != 0)
        {
            std::cout << ", ";
        }

        std::cout << solution.assignment[job];
    }

    std::cout << ']';
}

} // namespace

int main()
{
    using namespace easylocal::mwe::assignment;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;
    using easylocal::search::FirstImprovement;
    using easylocal::search::FirstImprovementTermination;

    const AssignmentInstance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const AssignmentSolution initial_solution{
        .assignment = {0, 0, 1},
    };

    auto runner =
        Runner{FirstImprovement{{.max_evaluations = 100}}}
        | (solution_manager<AssignmentSolutionManager>()
           | component<CapacityCostComponent>())
        | (neighborhood<ReassignJobNeighborhoodExplorer>()
           | delta<
                 CapacityCostComponent,
                 ReassignCapacityDeltaEvaluator>());

    const auto result = runner.bind(instance).run(initial_solution);

    std::cout << "initial solution: ";
    print_solution(initial_solution);
    std::cout << '\n';

    std::cout << "final solution:   ";
    print_solution(result.solution);
    std::cout << '\n';

    std::cout << "final cost: overload=" << result.cost.get<0>()
              << ", overloaded_machines=" << result.cost.get<1>() << '\n';
    std::cout << "evaluations: " << result.evaluations << '\n';
    std::cout << "termination: "
              << (result.termination == FirstImprovementTermination::local_optimum
                      ? "local optimum"
                      : "evaluation budget exhausted")
              << '\n';

    return 0;
}
