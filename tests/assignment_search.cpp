#include <easylocal/runner.hpp>
#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>

#include "capacity_delta.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace easylocal::mwe::assignment;
using easylocal::search::BestImprovement;
using easylocal::search::BestImprovementTermination;
using easylocal::search::FirstImprovement;
using easylocal::search::FirstImprovementTermination;

[[nodiscard]]
auto default_solution_manager_recipe()
{
    return easylocal::solution_manager<AssignmentSolutionManager>()
         | easylocal::component<CapacityCostComponent>()
         | easylocal::aggregator([](const CapacityValue& capacity) {
               return AssignmentCostAggregator{}.hard(capacity);
           });
}

[[nodiscard]]
auto default_neighborhood_recipe()
{
    return easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
         | easylocal::delta<
               CapacityCostComponent,
               ReassignCapacityDeltaEvaluator>();
}

class AssignmentCardinalityComponent
{
public:
    using value_type = std::size_t;

    explicit AssignmentCardinalityComponent(const AssignmentInstance&) noexcept
    {
    }

    [[nodiscard]]
    auto evaluate(const AssignmentSolution& solution) const noexcept -> value_type
    {
        return solution.assignment.size();
    }
};

class FallbackSolutionManager : public AssignmentSolutionManager
{
public:
    using AssignmentSolutionManager::AssignmentSolutionManager;
};

struct FallbackAggregator
{
    [[nodiscard]]
    auto operator()(
        const CapacityValue& capacity,
        const std::size_t cardinality) const -> HardCost
    {
        return HardCost{
            capacity.total_overload,
            static_cast<std::int64_t>(cardinality),
        };
    }
};

class SingleMoveNeighborhoodExplorer
{
public:
    using instance_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    SingleMoveNeighborhoodExplorer(
        const AssignmentSolutionManager& solution_manager,
        const ReassignJobMove move,
        int& make_move_count) noexcept
        : solution_manager_{solution_manager},
          move_{move},
          make_move_count_{make_move_count}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const AssignmentInstance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution&) const
    {
        return std::views::single(move_);
    }

    void make_move(AssignmentSolution& solution, const ReassignJobMove& move) const noexcept
    {
        ++make_move_count_.get();
        solution.assignment[move.job] = move.destination;
    }

private:
    const AssignmentSolutionManager& solution_manager_;
    ReassignJobMove move_;
    std::reference_wrapper<int> make_move_count_;
};

class ConstructionTrackingNeighborhoodExplorer
{
public:
    using instance_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    ConstructionTrackingNeighborhoodExplorer(
        const AssignmentSolutionManager& solution_manager,
        int& construction_marker) noexcept
        : inner_{solution_manager}
    {
        ++construction_marker;
    }

    [[nodiscard]]
    auto instance() const noexcept -> const AssignmentInstance&
    {
        return inner_.instance();
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution& solution) const
    {
        return easylocal::moves(inner_, solution);
    }

    void make_move(AssignmentSolution& solution, const ReassignJobMove& move) const noexcept
    {
        inner_.make_move(solution, move);
    }

private:
    ReassignJobNeighborhoodExplorer inner_;
};

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    using namespace easylocal::mwe::assignment;
    using easylocal::Runner;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    using easylocal::component;
    using easylocal::delta;

    bool ok = true;

    const AssignmentInstance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const AssignmentSolution initial{
        .assignment = {0, 0, 1},
    };

    const ReassignJobMove relieving_move{
        .job = 1,
        .destination = 1,
    };
    const ReassignJobMove worsening_move{
        .job = 2,
        .destination = 0,
    };

    int delta_accept_make_moves = 0;
    auto delta_accept_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | default_solution_manager_recipe()
        | (neighborhood<SingleMoveNeighborhoodExplorer>(
               relieving_move,
               std::ref(delta_accept_make_moves))
           | delta<
                 CapacityCostComponent,
                 ReassignCapacityDeltaEvaluator>());

    const auto delta_accept_result =
        delta_accept_runner.bind(instance).run(initial);

    ok &= expect(
        delta_accept_result.cost == HardCost{0, 0},
        "automatic delta dispatch preserves the accepted candidate cost");
    ok &= expect(
        delta_accept_make_moves == 1,
        "all-delta candidate applies make_move only once, at acceptance");

    int delta_reject_make_moves = 0;
    auto delta_reject_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | default_solution_manager_recipe()
        | (neighborhood<SingleMoveNeighborhoodExplorer>(
               worsening_move,
               std::ref(delta_reject_make_moves))
           | delta<
                 CapacityCostComponent,
                 ReassignCapacityDeltaEvaluator>());

    const auto delta_reject_result =
        delta_reject_runner.bind(instance).run(initial);

    ok &= expect(
        delta_reject_result.solution.assignment == initial.assignment,
        "rejected all-delta candidate leaves the incumbent unchanged");
    ok &= expect(
        delta_reject_make_moves == 0,
        "rejected all-delta candidate never materializes an AssignmentSolution");

    int fallback_make_moves = 0;
    auto fallback_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | (solution_manager<FallbackSolutionManager>()
           | component<CapacityCostComponent>()
           | component<AssignmentCardinalityComponent>()
           | easylocal::aggregator(FallbackAggregator{}))
        | (neighborhood<SingleMoveNeighborhoodExplorer>(
               relieving_move,
               std::ref(fallback_make_moves))
           | delta<
                 CapacityCostComponent,
                 ReassignCapacityDeltaEvaluator>());

    const auto fallback_result =
        fallback_runner.bind(instance).run(initial);

    ok &= expect(
        fallback_result.cost == HardCost{0, 3},
        "missing component delta falls back to full evaluation of that component");
    ok &= expect(
        fallback_make_moves == 1,
        "fallback components share one materialized candidate and acceptance reuses it");

    int fallback_reject_make_moves = 0;
    auto fallback_reject_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | (solution_manager<FallbackSolutionManager>()
           | component<CapacityCostComponent>()
           | component<AssignmentCardinalityComponent>()
           | easylocal::aggregator(FallbackAggregator{}))
        | (neighborhood<SingleMoveNeighborhoodExplorer>(
               worsening_move,
               std::ref(fallback_reject_make_moves))
           | delta<
                 CapacityCostComponent,
                 ReassignCapacityDeltaEvaluator>());

    const auto fallback_reject_result =
        fallback_reject_runner.bind(instance).run(initial);

    ok &= expect(
        fallback_reject_result.solution.assignment == initial.assignment,
        "rejected mixed delta/fallback candidate leaves the incumbent unchanged");
    ok &= expect(
        fallback_reject_make_moves == 1,
        "mixed delta/fallback rejection materializes the candidate exactly once");

    int no_delta_accept_make_moves = 0;
    auto no_delta_accept_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | default_solution_manager_recipe()
        | neighborhood<SingleMoveNeighborhoodExplorer>(
              relieving_move,
              std::ref(no_delta_accept_make_moves));

    const auto no_delta_accept_result =
        no_delta_accept_runner.bind(instance).run(initial);

    ok &= expect(
        no_delta_accept_result.cost == HardCost{0, 0},
        "no-delta configuration falls back to full component evaluation");
    ok &= expect(
        no_delta_accept_make_moves == 1,
        "no-delta accepted candidate is materialized exactly once and then promoted");

    int no_delta_reject_make_moves = 0;
    auto no_delta_reject_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | default_solution_manager_recipe()
        | neighborhood<SingleMoveNeighborhoodExplorer>(
              worsening_move,
              std::ref(no_delta_reject_make_moves));

    const auto no_delta_reject_result =
        no_delta_reject_runner.bind(instance).run(initial);

    ok &= expect(
        no_delta_reject_result.solution.assignment == initial.assignment,
        "rejected no-delta candidate leaves the incumbent unchanged");
    ok &= expect(
        no_delta_reject_make_moves == 1,
        "no-delta rejection still materializes the candidate exactly once for evaluation");

    int construction_marker = 0;
    auto construction_tracked_runner =
        Runner{FirstImprovement{{.max_evaluations = 8}}}
        | default_solution_manager_recipe()
        | (neighborhood<ConstructionTrackingNeighborhoodExplorer>(
               std::ref(construction_marker))
           | delta<
                 CapacityCostComponent,
                 ReassignCapacityDeltaEvaluator>());

    ok &= expect(
        construction_marker == 0,
        "service recipes do not construct instance-bound services eagerly");

    auto bound_construction_tracked_runner =
        construction_tracked_runner.bind(instance);

    ok &= expect(
        construction_marker == 1,
        "bind(instance) materializes the configured service graph exactly once");

    const auto construction_tracked_result =
        bound_construction_tracked_runner.run(initial);

    ok &= expect(
        construction_tracked_result.cost == HardCost{0, 0},
        "constructor arguments captured by the runner recipe reach the bound neighborhood");

    const auto run_with_budget =
        [&](const std::size_t max_evaluations, AssignmentSolution solution) {
            auto runner =
                Runner{FirstImprovement{{
                    .max_evaluations = max_evaluations,
                }}}
                | default_solution_manager_recipe()
                | default_neighborhood_recipe();

            return runner.bind(instance).run(std::move(solution));
        };

    const auto complete = run_with_budget(8, initial);

    ok &= expect(
        complete.solution.assignment ==
            std::vector<machine_id>{1, 0, 0},
        "first improvement follows the deterministic neighborhood order");
    ok &= expect(
        complete.cost == HardCost{0, 0},
        "complete search reaches the hand-computed local optimum cost");
    ok &= expect(
        complete.evaluations == 8,
        "evaluation count includes the initial full evaluation");
    ok &= expect(
        complete.termination == FirstImprovementTermination::local_optimum,
        "complete neighborhood scan certifies local optimality");

    const auto initial_only = run_with_budget(1, initial);

    ok &= expect(
        initial_only.solution.assignment == initial.assignment,
        "budget one leaves the initial solution unchanged");
    ok &= expect(
        initial_only.cost == HardCost{2, 1},
        "budget one still evaluates the initial solution");
    ok &= expect(
        initial_only.evaluations == 1,
        "budget one performs exactly one full evaluation");
    ok &= expect(
        initial_only.termination ==
            FirstImprovementTermination::evaluation_budget_exhausted,
        "non-empty neighborhood cannot be certified with exhausted budget");

    const auto one_improvement = run_with_budget(2, initial);

    ok &= expect(
        one_improvement.solution.assignment ==
            std::vector<machine_id>{1, 0, 1},
        "last available evaluation may still accept an improving move");
    ok &= expect(
        one_improvement.cost == HardCost{1, 1},
        "accepted move updates the returned current cost");
    ok &= expect(
        one_improvement.evaluations == 2,
        "accepted move consumes the final evaluation");
    ok &= expect(
        one_improvement.termination ==
            FirstImprovementTermination::evaluation_budget_exhausted,
        "improvement at the budget limit does not imply local optimality");

    const auto partial_scan = run_with_budget(4, initial);

    ok &= expect(
        partial_scan.solution.assignment ==
            std::vector<machine_id>{1, 0, 1},
        "non-improving candidate evaluations do not change the current solution");
    ok &= expect(
        partial_scan.evaluations == 4,
        "partial neighborhood scan stops exactly at the evaluation budget");
    ok &= expect(
        partial_scan.termination ==
            FirstImprovementTermination::evaluation_budget_exhausted,
        "partial neighborhood scan cannot certify local optimality");

    const AssignmentInstance single_machine_instance{
        .demand = {1, 2},
        .capacity = {10},
    };
    const AssignmentSolution single_machine_solution{
        .assignment = {0, 0},
    };

    const auto empty_neighborhood =
        (Runner{FirstImprovement{{.max_evaluations = 1}}}
         | default_solution_manager_recipe()
         | default_neighborhood_recipe())
            .bind(single_machine_instance)
            .run(single_machine_solution);

    ok &= expect(
        empty_neighborhood.evaluations == 1,
        "empty neighborhood needs no candidate evaluation");
    ok &= expect(
        empty_neighborhood.termination ==
            FirstImprovementTermination::local_optimum,
        "empty neighborhood is locally optimal even when the budget is exhausted");

    const auto run_best_with_budget =
        [&](const std::size_t max_evaluations, AssignmentSolution solution) {
            auto runner =
                Runner{BestImprovement{{
                    .max_evaluations = max_evaluations,
                }}}
                | default_solution_manager_recipe()
                | default_neighborhood_recipe();

            return runner.bind(instance).run(std::move(solution));
        };

    const auto best_complete = run_best_with_budget(7, initial);

    ok &= expect(
        best_complete.solution.assignment ==
            std::vector<machine_id>{0, 1, 1},
        "best improvement selects the best move in the complete neighborhood");
    ok &= expect(
        best_complete.cost == HardCost{0, 0},
        "best improvement reaches the hand-computed local optimum cost");
    ok &= expect(
        best_complete.evaluations == 7,
        "best improvement evaluates every move before accepting a step");
    ok &= expect(
        best_complete.termination == BestImprovementTermination::local_optimum,
        "complete best-improvement scan certifies local optimality");

    const auto best_initial_only = run_best_with_budget(1, initial);

    ok &= expect(
        best_initial_only.solution.assignment == initial.assignment,
        "best improvement with budget one leaves the initial solution unchanged");
    ok &= expect(
        best_initial_only.evaluations == 1,
        "best improvement budget one performs only the initial evaluation");
    ok &= expect(
        best_initial_only.termination ==
            BestImprovementTermination::evaluation_budget_exhausted,
        "best improvement cannot scan a non-empty neighborhood with budget one");

    const auto best_partial_scan = run_best_with_budget(3, initial);

    ok &= expect(
        best_partial_scan.solution.assignment == initial.assignment,
        "partial best-improvement scan does not accept a best-so-far move");
    ok &= expect(
        best_partial_scan.cost == HardCost{2, 1},
        "partial best-improvement scan preserves the incumbent cost");
    ok &= expect(
        best_partial_scan.evaluations == 3,
        "partial best-improvement scan stops exactly at the budget");
    ok &= expect(
        best_partial_scan.termination ==
            BestImprovementTermination::evaluation_budget_exhausted,
        "partial scan cannot certify a best-improvement step");

    const auto best_one_step = run_best_with_budget(4, initial);

    ok &= expect(
        best_one_step.solution.assignment ==
            std::vector<machine_id>{0, 1, 1},
        "a complete neighborhood scan may consume the final evaluation and accept its best move");
    ok &= expect(
        best_one_step.cost == HardCost{0, 0},
        "completed best-improvement step updates the incumbent cost");
    ok &= expect(
        best_one_step.evaluations == 4,
        "one complete best-improvement step uses the initial plus three candidate evaluations");
    ok &= expect(
        best_one_step.termination ==
            BestImprovementTermination::evaluation_budget_exhausted,
        "accepted best move at the budget limit does not certify the next neighborhood");

    const auto best_empty_neighborhood =
        (Runner{BestImprovement{{.max_evaluations = 1}}}
         | default_solution_manager_recipe()
         | default_neighborhood_recipe())
            .bind(single_machine_instance)
            .run(single_machine_solution);

    ok &= expect(
        best_empty_neighborhood.evaluations == 1,
        "best improvement needs no candidate evaluation for an empty neighborhood");
    ok &= expect(
        best_empty_neighborhood.termination ==
            BestImprovementTermination::local_optimum,
        "best improvement recognizes an empty neighborhood as locally optimal");


    return ok ? 0 : 1;
}
