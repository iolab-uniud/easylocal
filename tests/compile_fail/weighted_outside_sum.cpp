#include "service_composition_fixture.hpp"

// cost::weighted gives the weight of a term of cost::sum, and is meaningless
// anywhere else.
int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] const auto recipe =
        solution_manager<BaseSolutionManager>()
        | easylocal::cost::in_order(
              easylocal::cost::weighted(component<ComponentA>(), 2),
              component<ComponentB>());
}
