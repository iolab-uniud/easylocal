// A SolutionManager keeps its Input by reference: it cannot be constructed
// from a temporary Input, which would dangle.
#include <easylocal/helpers/solution_manager.hpp>

struct Instance
{
};

struct Solution
{
};

class SolutionManager : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;
};

int main()
{
    const SolutionManager manager{Instance{}};
    (void)manager;
}
