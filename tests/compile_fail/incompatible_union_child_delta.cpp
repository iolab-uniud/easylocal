#include "service_composition_fixture.hpp"

#include <easylocal/helpers/neighborhood_union.hpp>

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
        | (solution_manager<BaseSolutionManager>() | easylocal::cost::apply(CostFunction{}, component<ComponentA>()))
        | neighborhood_union(
              neighborhood<Neighborhood>()
                  | delta<ComponentA, MalformedDeltaA>(),
              neighborhood<AnotherNeighborhood>());

    [[maybe_unused]] auto bound_runner = runner.bind(instance);
}
