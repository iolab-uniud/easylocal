#include <easylocal/config/parameters.hpp>
#include <easylocal/config/parameters_base.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/utils/input_base.hpp>

#include <cassert>
#include <ranges>
#include <type_traits>

namespace
{

struct Instance
{
    int marker{};
};

struct Solution
{
    int value{};
};

struct Move
{
    int delta{};
};

class SolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }
};

// A cost component on the same base: the Input reaches every service the same
// way.
class Component : public easylocal::input_base<Instance>
{
public:
    using input_base::input_base;

    [[nodiscard]]
    auto evaluate(const Solution& solution) const noexcept -> int
    {
        return solution.value + input().marker;
    }
};

struct Parameters
{
    int weight{1};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"weight", &Parameters::weight>("The weight"));
    }

    [[nodiscard]]
    auto validate() const -> easylocal::config::validation_result
    {
        return easylocal::config::check_schema(*this);
    }
};

// A configurable class on the parameter base: parameters_type comes from it.
class Function : public easylocal::parameters_base<Parameters>
{
public:
    using parameters_base::parameters_base;

    [[nodiscard]]
    auto operator()(int cost) const noexcept -> int
    {
        return cost * parameters().weight;
    }
};

class NeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    static auto moves(const Solution&)
    {
        return std::views::single(Move{1});
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }

    static void make_move(Solution& solution, const Move& move) noexcept
    {
        solution.value += move.delta;
    }

    [[nodiscard]]
    auto protected_solution_manager_address() const noexcept
        -> const SolutionManager*
    {
        return &solution_manager_;
    }
};

static_assert(std::same_as<SolutionManager::input_type, Instance>);
static_assert(std::same_as<SolutionManager::input_type, Instance>);
static_assert(std::same_as<SolutionManager::solution_type, Solution>);
static_assert(std::same_as<NeighborhoodExplorer::solution_manager_type, SolutionManager>);
static_assert(std::same_as<NeighborhoodExplorer::input_type, Instance>);
static_assert(std::same_as<NeighborhoodExplorer::input_type, Instance>);
static_assert(std::same_as<NeighborhoodExplorer::solution_type, Solution>);
static_assert(std::same_as<NeighborhoodExplorer::move_type, Move>);
static_assert(!std::is_polymorphic_v<SolutionManager>);
static_assert(!std::is_polymorphic_v<NeighborhoodExplorer>);
static_assert(std::same_as<Component::input_type, Instance>);
static_assert(std::same_as<Function::parameters_type, Parameters>);
// The Input is kept by pointer, so a service built on it stays assignable.
static_assert(std::is_copy_assignable_v<Component>);
// Not from a temporary Input, which would dangle.
static_assert(!std::constructible_from<Component, Instance&&>);
static_assert(!std::constructible_from<SolutionManager, Instance&&>);
static_assert(easylocal::base_solution_manager<SolutionManager>);
static_assert(easylocal::neighborhood_explorer_for<
              NeighborhoodExplorer,
              SolutionManager>);

} // namespace

int main()
{
    const Instance instance{.marker = 42};
    const SolutionManager solution_manager{instance};
    const NeighborhoodExplorer neighborhood{solution_manager};

    assert(&solution_manager.input() == &instance);
    assert(&solution_manager.input() == &instance);

    assert(neighborhood.protected_solution_manager_address() == &solution_manager);
    assert(&neighborhood.input() == &instance);
    assert(&neighborhood.input() == &instance);

    const Component component{instance};
    assert(&component.input() == &instance);
    assert(component.evaluate(Solution{.value = 1}) == 43);

    const Function function{Parameters{.weight = 3}};
    assert(function.parameters().weight == 3);
    assert(function(2) == 6);

    return 0;
}
