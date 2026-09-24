#include "best_improvement.hpp"
#include "first_improvement.hpp"
#include "neighborhood_explorer.hpp"
#include "random_first_improvement.hpp"
#include <easylocal/runner.hpp>
#include "solution_manager.hpp"

#include <concepts>
#include <functional>
#include <iostream>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace easylocal::mwe::assignment;

template<class T>
concept CanAddSolutionManager = requires(T runner) {
    std::move(runner).template with_solution_manager<SolutionManager>();
};

template<class T>
concept CanAddNeighborhood = requires(T runner) {
    std::move(runner).template with_neighborhood<NeighborhoodExplorer>();
};

template<class T>
concept CanBindInstance = requires(T runner, const Instance& instance) {
    std::move(runner).bind(instance);
};

template<class T>
concept CanRun = requires(T& runner, Solution solution) {
    runner.run(std::move(solution));
};

template<class T, class RNG>
concept CanRunWithRng = requires(T& runner, Solution solution, RNG& rng) {
    runner.run(std::move(solution), rng);
};

class ConfiguredNeighborhoodExplorer
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    ConfiguredNeighborhoodExplorer(
        SolutionManager& solution_manager,
        int& construction_marker) noexcept
        : inner_{solution_manager}
    {
        ++construction_marker;
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return inner_.instance();
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const
    {
        return inner_.moves(solution);
    }

    void make_move(Solution& solution, const Move& move) const noexcept
    {
        inner_.make_move(solution, move);
    }

private:
    NeighborhoodExplorer inner_;
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

    using NakedRunner = Runner<FirstImprovement>;
    using RunnerWithSM = decltype(
        std::declval<NakedRunner&&>()
            .template with_solution_manager<SolutionManager>());
    using ConfiguredRunner = decltype(
        std::declval<RunnerWithSM&&>()
            .template with_neighborhood<NeighborhoodExplorer>());
    using BoundFirstRunner = decltype(
        std::declval<ConfiguredRunner&&>().bind(
            std::declval<const Instance&>()));

    using PipedConfiguredRunner = decltype(
        Runner{FirstImprovement{{.max_evaluations = 1}}}
        | solution_manager<SolutionManager>()
        | neighborhood<NeighborhoodExplorer>());

    static_assert(std::same_as<ConfiguredRunner, PipedConfiguredRunner>);

    static_assert(CanAddSolutionManager<NakedRunner>);
    static_assert(!CanAddNeighborhood<NakedRunner>);
    static_assert(!CanBindInstance<NakedRunner>);
    static_assert(!CanRun<NakedRunner>);
    static_assert(!CanRunWithRng<NakedRunner, std::mt19937>);

    static_assert(!CanAddSolutionManager<RunnerWithSM>);
    static_assert(CanAddNeighborhood<RunnerWithSM>);
    static_assert(!CanBindInstance<RunnerWithSM>);
    static_assert(!CanRun<RunnerWithSM>);
    static_assert(!CanRunWithRng<RunnerWithSM, std::mt19937>);

    static_assert(!CanAddSolutionManager<ConfiguredRunner>);
    static_assert(!CanAddNeighborhood<ConfiguredRunner>);
    static_assert(CanBindInstance<ConfiguredRunner>);
    static_assert(!CanRun<ConfiguredRunner>);
    static_assert(!CanRunWithRng<ConfiguredRunner, std::mt19937>);

    static_assert(std::same_as<
        BoundFirstRunner::solution_manager_type,
        SolutionManager>);
    static_assert(std::same_as<
        BoundFirstRunner::neighborhood_explorer_type,
        NeighborhoodExplorer>);
    static_assert(!std::copy_constructible<BoundFirstRunner>);
    static_assert(!std::movable<BoundFirstRunner>);
    static_assert(CanRun<BoundFirstRunner>);
    static_assert(!CanRunWithRng<BoundFirstRunner, std::mt19937>);

    using NakedBestRunner = Runner<BestImprovement>;
    using BestRunnerWithSM = decltype(
        std::declval<NakedBestRunner&&>()
            .template with_solution_manager<SolutionManager>());
    using ConfiguredBestRunner = decltype(
        std::declval<BestRunnerWithSM&&>()
            .template with_neighborhood<NeighborhoodExplorer>());
    using BoundBestRunner = decltype(
        std::declval<ConfiguredBestRunner&&>().bind(
            std::declval<const Instance&>()));

    static_assert(CanAddSolutionManager<NakedBestRunner>);
    static_assert(CanAddNeighborhood<BestRunnerWithSM>);
    static_assert(CanBindInstance<ConfiguredBestRunner>);
    static_assert(CanRun<BoundBestRunner>);
    static_assert(!CanRunWithRng<BoundBestRunner, std::mt19937>);

    using NakedRandomRunner = Runner<RandomFirstImprovement>;
    using RandomRunnerWithSM = decltype(
        std::declval<NakedRandomRunner&&>()
            .template with_solution_manager<SolutionManager>());
    using ConfiguredRandomRunner = decltype(
        std::declval<RandomRunnerWithSM&&>()
            .template with_neighborhood<NeighborhoodExplorer>());
    using BoundRandomRunner = decltype(
        std::declval<ConfiguredRandomRunner&&>().bind(
            std::declval<const Instance&>()));

    static_assert(CanAddSolutionManager<NakedRandomRunner>);
    static_assert(CanAddNeighborhood<RandomRunnerWithSM>);
    static_assert(CanBindInstance<ConfiguredRandomRunner>);
    static_assert(!CanRun<BoundRandomRunner>);
    static_assert(CanRunWithRng<BoundRandomRunner, std::mt19937>);

    bool ok = true;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };

    const Solution initial{
        .assignment = {0, 0, 1},
    };

    int construction_marker = 0;
    auto configured_services =
        Runner{FirstImprovement{{.max_evaluations = 8}}}
            .with_solution_manager<SolutionManager>()
            .with_neighborhood<ConfiguredNeighborhoodExplorer>(
                std::ref(construction_marker));

    ok &= expect(
        construction_marker == 0,
        "service recipes do not construct instance-bound services eagerly");

    auto configured_bound = configured_services.bind(instance);

    ok &= expect(
        construction_marker == 1,
        "bind(instance) materializes the configured service graph exactly once");

    const auto configured_result = configured_bound.run(initial);

    ok &= expect(
        configured_result.cost == Cost{0, 0},
        "constructor arguments captured by the runner recipe reach the bound neighborhood");

    const auto run_with_budget =
        [&](const std::size_t max_evaluations, Solution solution) {
            auto runner =
                Runner{FirstImprovement{{
                    .max_evaluations = max_evaluations,
                }}}
                    .with_solution_manager<SolutionManager>()
                    .with_neighborhood<NeighborhoodExplorer>();

            return runner.bind(instance).run(std::move(solution));
        };

    const auto complete = run_with_budget(8, initial);

    ok &= expect(
        complete.solution.assignment ==
            std::vector<machine_id>{1, 0, 0},
        "first improvement follows the deterministic neighborhood order");
    ok &= expect(
        complete.cost == Cost{0, 0},
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
        initial_only.cost == Cost{2, 1},
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
        one_improvement.cost == Cost{1, 1},
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
    const Solution single_machine_solution{
        .assignment = {0, 0},
    };

    const auto empty_neighborhood =
        (Runner{FirstImprovement{{.max_evaluations = 1}}}
         | solution_manager<SolutionManager>()
         | neighborhood<NeighborhoodExplorer>())
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
        [&](const std::size_t max_evaluations, Solution solution) {
            auto runner =
                Runner{BestImprovement{{
                    .max_evaluations = max_evaluations,
                }}}
                | solution_manager<SolutionManager>()
                | neighborhood<NeighborhoodExplorer>();

            return runner.bind(instance).run(std::move(solution));
        };

    const auto best_complete = run_best_with_budget(7, initial);

    ok &= expect(
        best_complete.solution.assignment ==
            std::vector<machine_id>{0, 1, 1},
        "best improvement selects the best move in the complete neighborhood");
    ok &= expect(
        best_complete.cost == Cost{0, 0},
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
        best_partial_scan.cost == Cost{2, 1},
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
        best_one_step.cost == Cost{0, 0},
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
         | solution_manager<SolutionManager>()
         | neighborhood<NeighborhoodExplorer>())
            .bind(single_machine_instance)
            .run(single_machine_solution);

    ok &= expect(
        best_empty_neighborhood.evaluations == 1,
        "best improvement needs no candidate evaluation for an empty neighborhood");
    ok &= expect(
        best_empty_neighborhood.termination ==
            BestImprovementTermination::local_optimum,
        "best improvement recognizes an empty neighborhood as locally optimal");

    const auto run_random_with_budget =
        [&](const std::size_t max_evaluations, Solution solution, auto& rng) {
            auto runner =
                Runner{RandomFirstImprovement{{
                    .max_evaluations = max_evaluations,
                }}}
                    .with_solution_manager<SolutionManager>()
                    .with_neighborhood<NeighborhoodExplorer>();

            return runner.bind(instance).run(std::move(solution), rng);
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
        random_complete_a.cost == Cost{0, 0},
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
        (Runner{RandomFirstImprovement{{.max_evaluations = 1}}}
         | solution_manager<SolutionManager>()
         | neighborhood<NeighborhoodExplorer>())
            .bind(single_machine_instance)
            .run(single_machine_solution, random_empty_rng);

    ok &= expect(
        random_empty_neighborhood.evaluations == 1,
        "random first improvement needs no candidate evaluation for an empty neighborhood");
    ok &= expect(
        random_empty_neighborhood.termination ==
            RandomFirstImprovementTermination::local_optimum,
        "empty without-replacement random traversal certifies local optimality");

    return ok ? 0 : 1;
}
