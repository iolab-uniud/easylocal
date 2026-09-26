#include "service_composition_fixture.hpp"

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::aggregator;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    const Instance instance{};

    auto runner =
        Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>() | component<ComponentA>() | aggregator(CostAggregator{}))
        | (neighborhood<Neighborhood>()
           | delta<ComponentA, MissingDeltaEvaluateA>());

    [[maybe_unused]] auto bound_runner = runner.bind(instance);
}
