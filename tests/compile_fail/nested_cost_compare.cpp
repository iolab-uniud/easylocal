#include "service_composition_fixture.hpp"

#include <compare>

// The order of the cost is defined at the root of the expression: a compare
// below another node would be ignored.
namespace
{

struct Maximize
{
    [[nodiscard]]
    int operator()(const int value) const noexcept
    {
        return value;
    }

    [[nodiscard]]
    static std::partial_ordering compare(const int lhs, const int rhs) noexcept
    {
        return rhs <=> lhs;
    }
};

} // namespace

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] const auto recipe = solution_manager<BaseSolutionManager>()
        | easylocal::cost::in_order(
            easylocal::cost::apply(Maximize{}, component<ComponentA>()),
            component<ComponentB>());
}
