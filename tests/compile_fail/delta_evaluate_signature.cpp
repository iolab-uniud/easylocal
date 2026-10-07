// The delta_evaluate of a delta cost component is checked when the delta is
// attached to the explorer, before the recipe is bound.
#include "service_composition_fixture.hpp"

struct MutableSolutionDeltaA
{
    explicit MutableSolutionDeltaA(const compile_fail_fixture::Instance&) noexcept {}

    [[nodiscard]]
    int delta_evaluate(
        compile_fail_fixture::Solution&,
        const compile_fail_fixture::Move& move) const noexcept
    {
        return move.delta;
    }
};

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::delta;
    using easylocal::neighborhood;

    [[maybe_unused]] auto recipe =
        neighborhood<Neighborhood>() | delta<ComponentA, MutableSolutionDeltaA>();
}
