#include "service_composition_fixture.hpp"

namespace
{

struct NonAggregatableValue
{
    int value{};
};

struct NonAggregatableComponent
{
    explicit NonAggregatableComponent(const compile_fail_fixture::Instance&) noexcept {}

    [[nodiscard]]
    auto evaluate(const compile_fail_fixture::Solution& solution) const noexcept
        -> NonAggregatableValue
    {
        return {solution.value};
    }
};

} // namespace

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] auto runner =
        Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>()
           | component<NonAggregatableComponent>());
}
