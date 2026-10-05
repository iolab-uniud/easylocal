// A delta cost component is attached by a layer that derives from the
// explorer, so a final explorer cannot take one.
#include "service_composition_fixture.hpp"

class FinalNeighborhood final : public compile_fail_fixture::Neighborhood
{
public:
    using Neighborhood::Neighborhood;
};

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::Runner;
    using easylocal::solution_manager;

    const Instance instance{};

    auto runner = Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>()
            | easylocal::cost::apply(CostFunction{}, component<ComponentA>()))
        | (neighborhood<FinalNeighborhood>() | delta<ComponentA, DeltaA>());

    [[maybe_unused]] auto bound_runner = runner.bind(instance);
}
