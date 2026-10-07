#include "service_composition_fixture.hpp"

namespace
{

struct DomainValue
{
    int value{};
};

struct DomainComponent
{
    explicit DomainComponent(const compile_fail_fixture::Instance&) noexcept {}

    [[nodiscard]]
    DomainValue evaluate(const compile_fail_fixture::Solution& solution) const noexcept
    {
        return {solution.value};
    }
};

} // namespace

// cost::sum adds numbers: a domain value is turned into one with cost::apply,
// it never needs arithmetic operators just to be summed.
int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] const auto recipe =
        solution_manager<BaseSolutionManager>()
        | easylocal::cost::sum(component<ComponentA>(), component<DomainComponent>());
}
