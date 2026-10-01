#include <easylocal/testing/check.hpp>
#include <easylocal/testing/cost_component.hpp>
#include <easylocal/testing/delta_evaluator.hpp>
#include <easylocal/testing/neighborhood.hpp>
#include <easylocal/testing/solution_manager.hpp>

#include "capacity_delta.hpp"
#include "cost_components.hpp"
#include "instance.hpp"
#include "neighborhood_explorer.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <cstdint>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>

namespace
{
using namespace easylocal::mwe::assignment;

struct AssignmentCheckData
{
    static auto instance() -> AssignmentInstance
    {
        return {
            .demand = {4, 3, 2},
            .capacity = {5, 5},
        };
    }

    static auto solution(const AssignmentInstance&) -> AssignmentSolution
    {
        return {.assignment = {0, 0, 1}};
    }

    static constexpr std::size_t random_samples = 16;
};

struct AssignmentSolutionManagerCheck : AssignmentCheckData
{
    using solution_manager = AssignmentSolutionManager;
};

struct CapacityComponentCheck : AssignmentCheckData
{
    using solution_manager = AssignmentSolutionManager;
    using component = CapacityCostComponent;
};

struct LoadImbalanceComponentCheck : AssignmentCheckData
{
    using solution_manager = AssignmentSolutionManager;
    using component = LoadImbalanceCostComponent;
};

struct CapacityDeltaCheck : AssignmentCheckData
{
    using neighborhood = ReassignJobNeighborhoodExplorer;
    using component = CapacityCostComponent;
    using delta_evaluator = ReassignCapacityDeltaEvaluator;
};

struct AssignmentNeighborhoodCheck : AssignmentCheckData
{
    using neighborhood = ReassignJobNeighborhoodExplorer;
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

struct ColocatedDeltaCheck
{
    using neighborhood = TinyNeighborhood;
    using component = TinyColocatedComponent;

    static auto instance() -> TinyInstance
    {
        return {};
    }

    static auto solution(const TinyInstance&) -> TinySolution
    {
        return {.value = 7};
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

    const auto sm_report = easylocal::testing::check_solution_manager<
        AssignmentSolutionManagerCheck>();
    const auto component_report = easylocal::testing::check_cost_component<
        CapacityComponentCheck>();
    const auto scalar_component_report = easylocal::testing::check_cost_component<
        LoadImbalanceComponentCheck>();
    const auto delta_report = easylocal::testing::check_delta_evaluator<
        CapacityDeltaCheck>();
    const auto colocated_delta_report = easylocal::testing::check_delta_evaluator<
        ColocatedDeltaCheck>();
    const auto neighborhood_report = easylocal::testing::check_neighborhood<
        AssignmentNeighborhoodCheck>();

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
    if (easylocal::testing::run_checks(
            run_output,
            easylocal::testing::check_solution_manager<AssignmentSolutionManagerCheck>(),
            easylocal::testing::check_cost_component<CapacityComponentCheck>(),
            easylocal::testing::check_delta_evaluator<CapacityDeltaCheck>(),
            easylocal::testing::check_neighborhood<AssignmentNeighborhoodCheck>()) != 0)
    {
        return 7;
    }

    if (run_output.str().find("SolutionManager") == std::string::npos ||
        run_output.str().find("cost component") == std::string::npos ||
        run_output.str().find("delta evaluator") == std::string::npos)
    {
        return 8;
    }

    return 0;
}
