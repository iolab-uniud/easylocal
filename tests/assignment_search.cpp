#include "best_improvement.hpp"
#include "first_improvement.hpp"
#include "neighborhood_explorer.hpp"
#include "random_first_improvement.hpp"
#include "runner.hpp"
#include "solution_manager.hpp"

#include <concepts>
#include <iostream>
#include <random>
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

template<class T, class RNG>
concept CanRunWithRng = requires(
    T runner,
    const Instance& instance,
    Solution solution,
    RNG& rng)
{
    runner.run(instance, std::move(solution), rng);
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

    using BoundFirstSM = decltype(
        std::declval<NakedRunner&&>().bind(
            std::declval<const SolutionManager&>()));
    using BoundFirstNeighborhood = decltype(
        std::declval<RunnerWithSM&&>().bind(
            std::declval<const NeighborhoodExplorer&>()));

    static_assert(std::same_as<BoundFirstSM, RunnerWithSM>);
    static_assert(std::same_as<BoundFirstNeighborhood, EquippedRunner>);

    static_assert(CanBindSolutionManager<NakedRunner>);
    static_assert(!CanBindNeighborhood<NakedRunner>);
    static_assert(!CanRun<NakedRunner>);
    static_assert(!CanRunWithRng<NakedRunner, std::mt19937>);

    static_assert(!CanBindSolutionManager<RunnerWithSM>);
    static_assert(CanBindNeighborhood<RunnerWithSM>);
    static_assert(!CanRun<RunnerWithSM>);
    static_assert(!CanRunWithRng<RunnerWithSM, std::mt19937>);

    static_assert(!CanBindSolutionManager<EquippedRunner>);
    static_assert(!CanBindNeighborhood<EquippedRunner>);
    static_assert(CanRun<EquippedRunner>);
    static_assert(!CanRunWithRng<EquippedRunner, std::mt19937>);

    using NakedBestRunner = Runner<BestImprovement>;
    using BestRunnerWithSM = Runner<BestImprovement, SolutionManager>;
    using EquippedBestRunner =
        Runner<BestImprovement, SolutionManager, NeighborhoodExplorer>;

    using BoundBestSM = decltype(
        std::declval<NakedBestRunner&&>().bind(
            std::declval<const SolutionManager&>()));
    using BoundBestNeighborhood = decltype(
        std::declval<BestRunnerWithSM&&>().bind(
            std::declval<const NeighborhoodExplorer&>()));

    static_assert(std::same_as<BoundBestSM, BestRunnerWithSM>);
    static_assert(std::same_as<BoundBestNeighborhood, EquippedBestRunner>);

    static_assert(CanBindSolutionManager<NakedBestRunner>);
    static_assert(!CanBindNeighborhood<NakedBestRunner>);
    static_assert(!CanRun<NakedBestRunner>);
    static_assert(!CanRunWithRng<NakedBestRunner, std::mt19937>);

    static_assert(!CanBindSolutionManager<BestRunnerWithSM>);
    static_assert(CanBindNeighborhood<BestRunnerWithSM>);
    static_assert(!CanRun<BestRunnerWithSM>);
    static_assert(!CanRunWithRng<BestRunnerWithSM, std::mt19937>);

    static_assert(!CanBindSolutionManager<EquippedBestRunner>);
    static_assert(!CanBindNeighborhood<EquippedBestRunner>);
    static_assert(CanRun<EquippedBestRunner>);
    static_assert(!CanRunWithRng<EquippedBestRunner, std::mt19937>);

    using NakedRandomRunner = Runner<RandomFirstImprovement>;
    using RandomRunnerWithSM = Runner<RandomFirstImprovement, SolutionManager>;
    using EquippedRandomRunner =
        Runner<RandomFirstImprovement, SolutionManager, NeighborhoodExplorer>;

    using BoundRandomSM = decltype(
        std::declval<NakedRandomRunner&&>().bind(
            std::declval<const SolutionManager&>()));
    using BoundRandomNeighborhood = decltype(
        std::declval<RandomRunnerWithSM&&>().bind(
            std::declval<const NeighborhoodExplorer&>()));

    static_assert(std::same_as<BoundRandomSM, RandomRunnerWithSM>);
    static_assert(
        std::same_as<BoundRandomNeighborhood, EquippedRandomRunner>);

    static_assert(CanBindSolutionManager<NakedRandomRunner>);
    static_assert(!CanBindNeighborhood<NakedRandomRunner>);
    static_assert(!CanRun<NakedRandomRunner>);
    static_assert(!CanRunWithRng<NakedRandomRunner, std::mt19937>);

    static_assert(!CanBindSolutionManager<RandomRunnerWithSM>);
    static_assert(CanBindNeighborhood<RandomRunnerWithSM>);
    static_assert(!CanRun<RandomRunnerWithSM>);
    static_assert(!CanRunWithRng<RandomRunnerWithSM, std::mt19937>);

    static_assert(!CanBindSolutionManager<EquippedRandomRunner>);
    static_assert(!CanBindNeighborhood<EquippedRandomRunner>);
    static_assert(!CanRun<EquippedRandomRunner>);
    static_assert(CanRunWithRng<EquippedRandomRunner, std::mt19937>);

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

    const auto run_best_with_budget =
        [&](const std::size_t max_evaluations, Solution solution) {
            return Runner{BestImprovement{{
                              .max_evaluations = max_evaluations,
                          }}}
                .bind(solution_manager)
                .bind(neighborhood)
                .run(instance, std::move(solution));
        };

    const auto best_complete = run_best_with_budget(7, initial);

    ok &= expect(
        best_complete.solution.assignment ==
            std::vector<machine_id>{0, 1, 1},
        "best improvement selects the best move in the complete neighborhood");
    ok &= expect(
        best_complete.cost == Cost{0},
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
        best_partial_scan.cost == Cost{2},
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
        best_one_step.cost == Cost{0},
        "completed best-improvement step updates the incumbent cost");
    ok &= expect(
        best_one_step.evaluations == 4,
        "one complete best-improvement step uses the initial plus three candidate evaluations");
    ok &= expect(
        best_one_step.termination ==
            BestImprovementTermination::evaluation_budget_exhausted,
        "accepted best move at the budget limit does not certify the next neighborhood");

    const auto best_empty_neighborhood =
        Runner{BestImprovement{{.max_evaluations = 1}}}
            .bind(single_machine_manager)
            .bind(single_machine_neighborhood)
            .run(single_machine_instance, single_machine_solution);

    ok &= expect(
        best_empty_neighborhood.evaluations == 1,
        "best improvement needs no candidate evaluation for an empty neighborhood");
    ok &= expect(
        best_empty_neighborhood.termination ==
            BestImprovementTermination::local_optimum,
        "best improvement recognizes an empty neighborhood as locally optimal");

    const auto run_random_with_budget =
        [&](const std::size_t max_evaluations, Solution solution, auto& rng) {
            return Runner{RandomFirstImprovement{{
                              .max_evaluations = max_evaluations,
                          }}}
                .bind(solution_manager)
                .bind(neighborhood)
                .run(instance, std::move(solution), rng);
        };

    std::mt19937 random_rng_a{12345};
    std::mt19937 random_rng_b{12345};

    const auto random_complete_a =
        run_random_with_budget(16, initial, random_rng_a);
    const auto random_complete_b =
        run_random_with_budget(16, initial, random_rng_b);

    ok &= expect(
        random_complete_a.solution.assignment ==
            random_complete_b.solution.assignment &&
            random_complete_a.cost == random_complete_b.cost &&
            random_complete_a.evaluations == random_complete_b.evaluations &&
            random_complete_a.termination == random_complete_b.termination,
        "random first improvement is reproducible for identical RNG state");
    ok &= expect(
        random_complete_a.cost == Cost{0},
        "random first improvement reaches a local optimum independently of random traversal order");
    ok &= expect(
        random_complete_a.termination ==
            RandomFirstImprovementTermination::local_optimum,
        "exhausting a without-replacement random traversal certifies local optimality");

    std::mt19937 random_budget_rng{7};
    const auto random_initial_only =
        run_random_with_budget(1, initial, random_budget_rng);

    ok &= expect(
        random_initial_only.solution.assignment == initial.assignment,
        "random first improvement with budget one leaves the initial solution unchanged");
    ok &= expect(
        random_initial_only.evaluations == 1,
        "random first improvement budget one performs only the initial evaluation");
    ok &= expect(
        random_initial_only.termination ==
            RandomFirstImprovementTermination::evaluation_budget_exhausted,
        "random first improvement is bounded by its evaluation budget");

    std::mt19937 random_empty_rng{99};
    const auto random_empty_neighborhood =
        Runner{RandomFirstImprovement{{.max_evaluations = 1}}}
            .bind(single_machine_manager)
            .bind(single_machine_neighborhood)
            .run(
                single_machine_instance,
                single_machine_solution,
                random_empty_rng);

    ok &= expect(
        random_empty_neighborhood.evaluations == 1,
        "random first improvement needs no candidate evaluation for an empty neighborhood");
    ok &= expect(
        random_empty_neighborhood.termination ==
            RandomFirstImprovementTermination::local_optimum,
        "empty without-replacement random traversal certifies local optimality");

    return ok ? 0 : 1;
}
