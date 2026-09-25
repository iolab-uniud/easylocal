#include "service_composition_fixture.hpp"

#include <easylocal/neighborhood_union.hpp>

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::neighborhood_union;
    using easylocal::solution_manager;

    const Instance instance{};

    auto runner =
        Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>() | component<ComponentA>())
        | neighborhood_union(
              neighborhood<Neighborhood>()
                  | delta<ComponentA, MalformedDeltaA>(),
              neighborhood<Neighborhood>());

    [[maybe_unused]] auto bound = runner.bind(instance);
}
