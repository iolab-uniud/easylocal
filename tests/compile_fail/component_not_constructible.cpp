#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    const Instance instance{};

    auto runner =
        Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>()
           | component<NonConstructibleComponent>())
        | neighborhood<Neighborhood>();

    [[maybe_unused]] auto bound_runner = runner.bind(instance);
}
