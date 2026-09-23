#include "first_improvement.hpp"
#include "neighborhood_explorer.hpp"
#include "runner.hpp"
#include "solution_manager.hpp"

#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace easylocal::mwe::assignment;

template<class T>
concept CanBindSolutionManager = requires(T runner, const SolutionManager& sm) {
    std::move(runner).bind(sm);
};

template<class T>
concept CanBindNeighborhood = requires(T runner, const NeighborhoodExplorer& nhe) {
    std::move(runner).bind(nhe);
};

template<class T>
concept CanRun = requires(T runner, const Instance& instance, Solution solution) {
    runner.run(instance, std::move(solution));
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

    using NakedRunner = Runner<FirstImprovement>;
    using RunnerWithSM = Runner<FirstImprovement, SolutionManager>;
    using EquippedRunner =
        Runner<FirstImprovement, SolutionManager, NeighborhoodExplorer>;

    static_assert(CanBindSolutionManager<NakedRunner>);
    static_assert(!CanBindNeighborhood<NakedRunner>);
    static_assert(!CanRun<NakedRunner>);
    static_assert(!CanBindSolutionManager<RunnerWithSM>);
    static_assert(CanBindNeighborhood<RunnerWithSM>);
    static_assert(!CanRun<RunnerWithSM>);
    static_assert(CanRun<EquippedRunner>);

    bool ok = true;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };
    const SolutionManager solution_manager{instance};
    const NeighborhoodExplorer neighborhood{solution_manager};

    const Solution initial{
        .assignment = {0, 0, 1},
    };

    const auto run_with_budget =
        [&](const std::size_t max_evaluations, Solution solution) {
            return Runner{FirstImprovement{{
                              .max_evaluations = max_evaluations,
                          }}}
                .bind(solution_manager)
                .bind(neighborhood)
                .run(instance, std::move(solution));
        };

    const auto complete = run_with_budget(8, initial);

    ok &= expect(
        complete.solution.assignment ==
            std::vector<machine_id>{1, 0, 0},
        "first improvement follows the deterministic neighborhood order");
    ok &= expect(
        complete.cost == Cost{0},
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
        initial_only.cost == Cost{2},
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
        one_improvement.cost == Cost{1},
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

    const Instance single_machine_instance{
        .demand = {1, 2},
        .capacity = {10},
    };
    const SolutionManager single_machine_manager{single_machine_instance};
    const NeighborhoodExplorer single_machine_neighborhood{
        single_machine_manager};
    const Solution single_machine_solution{
        .assignment = {0, 0},
    };

    const auto empty_neighborhood =
        Runner{FirstImprovement{{.max_evaluations = 1}}}
            .bind(single_machine_manager)
            .bind(single_machine_neighborhood)
            .run(single_machine_instance, single_machine_solution);

    ok &= expect(
        empty_neighborhood.evaluations == 1,
        "empty neighborhood needs no candidate evaluation");
    ok &= expect(
        empty_neighborhood.termination ==
            FirstImprovementTermination::local_optimum,
        "empty neighborhood is locally optimal even when the budget is exhausted");

    return ok ? 0 : 1;
}
