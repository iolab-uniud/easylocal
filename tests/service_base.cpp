#include <easylocal/runners/runner.hpp>
#include <easylocal/helpers/service_base.hpp>

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
    using cost_type = int;

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]]
    static auto evaluate(const Solution& solution) noexcept -> cost_type
    {
        return solution.value;
    }

    [[nodiscard]]
    auto protected_input_address() const noexcept -> const Instance*
    {
        return &input_;
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
static_assert(easylocal::evaluable_solution_manager<SolutionManager>);
static_assert(easylocal::detail::runner_neighborhood_explorer<
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
    assert(solution_manager.protected_input_address() == &instance);

    assert(neighborhood.protected_solution_manager_address() == &solution_manager);
    assert(&neighborhood.input() == &instance);
    assert(&neighborhood.input() == &instance);

    return 0;
}
