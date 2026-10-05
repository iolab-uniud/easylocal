// A NeighborhoodExplorer keeps its SolutionManager by reference: it cannot be
// constructed from a temporary SolutionManager, which would dangle.
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>

struct Instance
{
};

struct Solution
{
};

struct Move
{
};

class SolutionManager : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;
};

class Neighborhood : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;
};

int main()
{
    const Instance instance{};
    const Neighborhood neighborhood{SolutionManager{instance}};
    (void)neighborhood;
}
