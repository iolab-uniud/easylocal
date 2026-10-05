// Recipe arguments that construct no SolutionManager are reported as such,
// without the hint about inherited constructors, which does not apply.
#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    const Instance instance{};
    const auto recipe =
        solution_manager<BaseSolutionManager>(42) | component<ComponentA>();
    [[maybe_unused]] const auto manager = recipe.construct(instance);
}
