// The configuration of a temporary runner would refer to it after it is gone:
// configuration() takes only a runner that stays in place.
#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::neighborhood;
    using easylocal::Runner;
    using easylocal::solution_manager;

    [[maybe_unused]] auto parameters =
        (Runner{Algorithm{}}
            | (solution_manager<BaseSolutionManager>() | component<ComponentA>())
            | neighborhood<Neighborhood>())
            .configuration();
}
