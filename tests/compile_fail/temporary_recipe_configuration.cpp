// The configuration of a temporary recipe would refer to it after it is gone:
// its configuration() is deleted on an rvalue.
#include "service_composition_fixture.hpp"

#include <easylocal/cost.hpp>

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] auto parameters =
        (solution_manager<BaseSolutionManager>()
            | easylocal::cost::sum(component<ComponentA>(), component<ComponentB>()))
            .configuration();
}
