// A cost component with parameters is configured under its name: one without a
// static name() does not compile.
#include "service_composition_fixture.hpp"

#include <easylocal/config/parameters.hpp>
#include <easylocal/cost.hpp>

struct WeightParameters
{
    int weight{1};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"weight", &WeightParameters::weight>(
                "Weight",
                easylocal::config::range(0, 10)));
    }

    [[nodiscard]] easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};

struct UnnamedComponent
{
    using parameters_type = WeightParameters;

    UnnamedComponent(const compile_fail_fixture::Instance&, const WeightParameters&) {}

    [[nodiscard]] static int evaluate(const compile_fail_fixture::Solution& solution)
    {
        return solution.value;
    }
};

int main()
{
    using namespace compile_fail_fixture;
    using easylocal::component;
    using easylocal::solution_manager;

    [[maybe_unused]] auto recipe =
        solution_manager<BaseSolutionManager>() | component<UnnamedComponent>();
}
