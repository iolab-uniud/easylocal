// cost::approximately: the search compares the costs of its child within a
// tolerance, keeps the hard components of a hard_soft child visible, and the
// hard projection compares within the same tolerance.
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/detail/cost_layer.hpp>
#include <easylocal/helpers/recipes.hpp>

#include <algorithm>
#include <iostream>
#include <string_view>
#include <utility>

namespace
{

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}

struct Instance
{
};

struct Solution
{
    double violation{};
    double length{};
};

class SolutionManager : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] static bool is_valid(const Solution&) noexcept
    {
        return true;
    }
};

struct Violation
{
    [[nodiscard]] static double evaluate(const Solution& solution) noexcept
    {
        return solution.violation;
    }
};

struct Length
{
    [[nodiscard]] static double evaluate(const Solution& solution) noexcept
    {
        return solution.length;
    }
};

using cost_type = easylocal::cost::hierarchical<double, double>;

} // namespace

int main()
{
    using easylocal::component;
    namespace cost = easylocal::cost;

    bool ok = true;
    const Instance instance;

    auto recipe = easylocal::solution_manager<SolutionManager>()
        | cost::approximately(
            cost::hard_soft(component<Violation>(), component<Length>()),
            {.relative = 1e-9, .absolute = 1e-9});
    const auto manager = recipe.construct(instance);
    using manager_type = std::remove_cvref_t<decltype(manager)>;

    const cost_type drifted{0.1 + 0.2 - 0.3, 0.1 + 0.2};
    const cost_type exact{0.0, 0.3};
    ok &= expect(
        drifted != exact && cost::equivalent(manager, drifted, exact)
            && cost::better_or_equivalent(manager, drifted, exact)
            && !cost::better(manager, exact, drifted),
        "costs equal within the tolerance are equivalent, neither better");
    ok &= expect(
        cost::better(manager, cost_type{0.0, 0.2}, exact)
            && cost::better(manager, exact, cost_type{1.0, 0.0}),
        "a cost is better by more than the tolerance, the hard cost first");

    static_assert(manager_type::has_hard_component_projection);
    static_assert(manager_type::hard_component_count == 1);
    const easylocal::detail::hard_cost_layer<manager_type> hard{manager};
    ok &= expect(
        cost::equivalent(hard, 0.1 + 0.2 - 0.3, 0.0) && !cost::better(hard, 1e-17, 0.0),
        "the hard projection compares the hard costs within the tolerance");

    const auto exact_manager =
        (easylocal::solution_manager<SolutionManager>()
            | cost::hard_soft(component<Violation>(), component<Length>()))
            .construct(instance);
    ok &= expect(
        !cost::equivalent(exact_manager, drifted, exact),
        "without cost::approximately the search compares exactly");

    const auto configuration = recipe.configuration();
    const auto parameters = configuration.parameters();
    const auto has = [&](const std::string_view path) {
        return std::ranges::any_of(parameters, [&](const auto& parameter) {
            return parameter.path == path;
        });
    };
    ok &= expect(
        has("cost.tolerance.relative") && has("cost.tolerance.absolute")
            && !has("cost.weights"),
        "the tolerance is a parameter of the cost expression, under cost in an app");

    return ok ? 0 : 1;
}
