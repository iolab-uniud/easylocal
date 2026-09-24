#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    const Instance instance{};

    auto runner =
        Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>() | component<ComponentA>())
        | (neighborhood<Neighborhood>()
           | delta<ComponentA, NonConstructibleDeltaA>());

    [[maybe_unused]] auto bound = runner.bind(instance);
}
