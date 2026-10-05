#include "service_composition_fixture.hpp"

#include <cstddef>

namespace
{

struct UnsignedComponent
{
    explicit UnsignedComponent(const compile_fail_fixture::Instance&) noexcept {}

    [[nodiscard]]
    auto evaluate(const compile_fail_fixture::Solution& solution) const noexcept
        -> std::size_t
    {
        return static_cast<std::size_t>(solution.value);
    }
};

} // namespace

// The difference of two unsigned costs wraps around, so a cost component
// returns a signed integer or a floating-point value.
int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::neighborhood;
    using easylocal::Runner;
    using easylocal::solution_manager;

    const Instance instance{};

    auto runner = Runner{Algorithm{}}
        | (solution_manager<BaseSolutionManager>() | component<UnsignedComponent>())
        | neighborhood<Neighborhood>();

    [[maybe_unused]] auto bound_runner = runner.bind(instance);
}
