#include "cost_components.hpp"
#include "instance.hpp"
#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/cost_component.hpp>
#include <easylocal/testing/delta_cost_component.hpp>
#include <easylocal/testing/neighborhood.hpp>
#include <easylocal/testing/solution_manager.hpp>

#include <cstdint>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>

namespace
{
using namespace assignment;

// A tolerance-based comparison, for fixtures whose values accumulate rounding.
struct WithinOne
{
    auto operator()(std::int64_t a, std::int64_t b) const -> bool
    {
        return a - b <= 1 && b - a <= 1;
    }
};

struct ProxyMove
{
    int delta{};
};

struct Move
{
    int delta{};
    Move() = default;
    explicit Move(const ProxyMove proxy) : delta{proxy.delta} {}
};

struct Solution
{
    int value{};
};

struct ProxyNeighborhood
{
    using move_type = Move;

    static auto is_valid(const Solution&, const Move& move) noexcept -> bool
    {
        return move.delta > 0;
    }

    static void make_move(Solution& solution, const Move& move) noexcept
    {
        solution.value += move.delta;
    }

    static auto moves(const Solution&)
    {
        return std::views::single(ProxyMove{1});
    }

    template<class RNG>
    static auto random_move(const Solution&, RNG&) -> std::optional<ProxyMove>
    {
        return ProxyMove{2};
    }
};

struct TinyInstance
{
};

struct TinySolution
{
    int value{};
};

struct TinyMove
{
    int delta{};
};

class TinySolutionManager
    : public easylocal::solution_manager_base<TinyInstance, TinySolution>
{
public:
    using solution_manager_base::solution_manager_base;

    static auto is_valid(const TinySolution&) noexcept -> bool
    {
        return true;
    }
};

class TinyNeighborhood
    : public easylocal::neighborhood_explorer_base<
          TinySolutionManager,
          TinyMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static auto is_valid(const TinySolution&, const TinyMove& move) noexcept -> bool
    {
        return move.delta == 1;
    }

    static auto moves(const TinySolution&)
    {
        return std::views::single(TinyMove{1});
    }

    static void make_move(TinySolution& solution, const TinyMove& move) noexcept
    {
        solution.value += move.delta;
    }
};

struct TinyColocatedComponent
{
    static auto evaluate(const TinySolution& solution) noexcept -> int
    {
        return solution.value;
    }

    static auto delta_evaluate(const TinySolution&, const TinyMove& move) noexcept -> int
    {
        return move.delta;
    }
};

} // namespace

int main()
{
    static_assert(easylocal::native_moves_neighborhood_for<ProxyNeighborhood, Solution>);
    static_assert(easylocal::random_neighborhood_for<
        ProxyNeighborhood,
        Solution,
        easylocal::testing::deterministic_rng>);

    ProxyNeighborhood proxy;
    Solution solution;
    for (auto&& raw : easylocal::moves(proxy, solution))
    {
        Move move{raw};
        if (move.delta != 1)
        {
            return 1;
        }
    }

    easylocal::testing::deterministic_rng rng{7, 11};
    if (rng() != 7 || rng() != 11 || rng() != 7)
    {
        return 2;
    }
    rng.reset();

    const auto random = easylocal::random_move(proxy, solution, rng);
    if (!random || random->delta != 2)
    {
        return 3;
    }

    namespace elt = easylocal::testing;

    const elt::fixture<AssignmentSolutionManager> assignment{
        AssignmentInstance{.demand = {4, 3, 2}, .capacity = {5, 5}},
        AssignmentSolution{.assignment = {0, 0, 1}},
        {.random_samples = 16},
    };
    const elt::fixture<TinySolutionManager> tiny{
        TinyInstance{},
        TinySolution{.value = 7}};

    const elt::fixture<AssignmentSolutionManager, WithinOne> tolerant{
        AssignmentInstance{.demand = {4, 3, 2}, .capacity = {5, 5}},
    };
    if (!tolerant.equivalent(3, 4) || tolerant.equivalent(3, 5))
        return 9;
    if (!elt::check_cost_component<LoadImbalanceCostComponent>(tolerant).passed())
        return 10;

    const auto sm_report = elt::check_solution_manager(assignment);
    const auto component_report =
        elt::check_cost_component<CapacityCostComponent>(assignment);
    const auto scalar_component_report =
        elt::check_cost_component<LoadImbalanceCostComponent>(assignment);
    const auto delta_report = elt::check_delta_cost_component<
        ReassignJobNeighborhoodExplorer,
        CapacityCostComponent,
        ReassignCapacityDelta>(assignment);
    const auto colocated_delta_report =
        elt::check_delta_cost_component<TinyNeighborhood, TinyColocatedComponent>(tiny);
    const auto neighborhood_report =
        elt::check_neighborhood<ReassignJobNeighborhoodExplorer>(assignment);

    if (!sm_report.passed() || !component_report.passed() ||
        !scalar_component_report.passed() || !delta_report.passed() ||
        !colocated_delta_report.passed() ||
        !neighborhood_report.passed())
    {
        return 4;
    }

    if (sm_report.checks() == 0 || component_report.checks() == 0 ||
        delta_report.checks() == 0 || neighborhood_report.checks() == 0)
    {
        return 5;
    }

    std::ostringstream output;
    easylocal::testing::print_report(output, neighborhood_report);
    if (output.str().find("checks passed") == std::string::npos ||
        output.str().find("NeighborhoodExplorer") == std::string::npos)
    {
        return 6;
    }

    std::ostringstream run_output;
    const TinyNeighborhood tiny_neighborhood{tiny.solution_manager()};
    if (easylocal::testing::run_checks(
            run_output,
            elt::check_solution_manager(assignment),
            elt::check_cost_component(
                assignment,
                CapacityCostComponent{assignment.input()}),
            elt::check_delta_cost_component(
                tiny,
                tiny_neighborhood,
                TinyColocatedComponent{}),
            elt::check_neighborhood(tiny, tiny_neighborhood))
        != 0)
    {
        return 7;
    }

    if (run_output.str().find("SolutionManager") == std::string::npos
        || run_output.str().find("cost component") == std::string::npos
        || run_output.str().find("delta cost component") == std::string::npos)
    {
        return 8;
    }

    return 0;
}
