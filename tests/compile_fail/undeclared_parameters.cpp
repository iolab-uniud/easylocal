// Only a parameters_type is configured: a cost::apply function that gives its
// parameters with configuration() does not compile.
#include "service_composition_fixture.hpp"

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>

struct SelfConfiguredFunction
{
    int offset{0};

    [[nodiscard]] easylocal::config::parameter_set configuration()
    {
        return {};
    }

    [[nodiscard]] int operator()(const int value) const noexcept
    {
        return value + offset;
    }
};

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] auto recipe = solution_manager<BaseSolutionManager>()
        | easylocal::cost::apply(SelfConfiguredFunction{}, component<ComponentA>());
}
