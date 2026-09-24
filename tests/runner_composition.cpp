#include <easylocal/runner.hpp>
#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/search/random_first_improvement.hpp>

#include "capacity_delta.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <concepts>
#include <random>
#include <utility>

namespace
{

using namespace easylocal::mwe::assignment;
using easylocal::search::BestImprovement;
using easylocal::search::FirstImprovement;
using easylocal::search::RandomFirstImprovement;

[[nodiscard]]
auto default_solution_manager_recipe()
{
    return easylocal::solution_manager<AssignmentSolutionManager>()
         | easylocal::component<CapacityCostComponent>();
}

[[nodiscard]]
auto default_neighborhood_recipe()
{
    return easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
         | easylocal::delta<
               CapacityCostComponent,
               ReassignCapacityDeltaEvaluator>();
}

template<class T>
concept CanConfigureSolutionManager = requires(T runner) {
    std::move(runner).with_solution_manager(
        default_solution_manager_recipe());
};

template<class T>
concept CanConfigureNeighborhood = requires(T runner) {
    std::move(runner).with_neighborhood(
        default_neighborhood_recipe());
};

template<class T>
concept CanBindInstance = requires(T runner, const AssignmentInstance& instance) {
    std::move(runner).bind(instance);
};

template<class T>
concept CanRun = requires(T& runner, AssignmentSolution solution) {
    runner.run(std::move(solution));
};

template<class T, class RNG>
concept CanRunWithRng = requires(T& runner, AssignmentSolution solution, RNG& rng) {
    runner.run(std::move(solution), rng);
};

} // namespace

int main()
{
    using easylocal::Runner;

    using DefaultSolutionManagerRecipe =
        decltype(default_solution_manager_recipe());
    using DefaultNeighborhoodRecipe =
        decltype(default_neighborhood_recipe());
    using ComposedSolutionManager =
        typename DefaultSolutionManagerRecipe::service_type;
    using ComposedNeighborhood =
        typename DefaultNeighborhoodRecipe::service_type;

    using AlgorithmOnlyFirstRunner = Runner<FirstImprovement>;
    using ManagerConfiguredFirstRunner = decltype(
        std::declval<AlgorithmOnlyFirstRunner&&>().with_solution_manager(
            default_solution_manager_recipe()));
    using FullyConfiguredFirstRunner = decltype(
        std::declval<ManagerConfiguredFirstRunner&&>().with_neighborhood(
            default_neighborhood_recipe()));
    using BoundFirstRunner = decltype(
        std::declval<FullyConfiguredFirstRunner&&>().bind(
            std::declval<const AssignmentInstance&>()));

    using PipelineConfiguredFirstRunner = decltype(
        Runner{FirstImprovement{{.max_evaluations = 1}}}
        | default_solution_manager_recipe()
        | default_neighborhood_recipe());

    static_assert(std::same_as<
        FullyConfiguredFirstRunner,
        PipelineConfiguredFirstRunner>);

    static_assert(CanConfigureSolutionManager<AlgorithmOnlyFirstRunner>);
    static_assert(!CanConfigureNeighborhood<AlgorithmOnlyFirstRunner>);
    static_assert(!CanBindInstance<AlgorithmOnlyFirstRunner>);
    static_assert(!CanRun<AlgorithmOnlyFirstRunner>);
    static_assert(!CanRunWithRng<AlgorithmOnlyFirstRunner, std::mt19937>);

    static_assert(!CanConfigureSolutionManager<ManagerConfiguredFirstRunner>);
    static_assert(CanConfigureNeighborhood<ManagerConfiguredFirstRunner>);
    static_assert(!CanBindInstance<ManagerConfiguredFirstRunner>);
    static_assert(!CanRun<ManagerConfiguredFirstRunner>);
    static_assert(!CanRunWithRng<ManagerConfiguredFirstRunner, std::mt19937>);

    static_assert(!CanConfigureSolutionManager<FullyConfiguredFirstRunner>);
    static_assert(!CanConfigureNeighborhood<FullyConfiguredFirstRunner>);
    static_assert(CanBindInstance<FullyConfiguredFirstRunner>);
    static_assert(!CanRun<FullyConfiguredFirstRunner>);
    static_assert(!CanRunWithRng<FullyConfiguredFirstRunner, std::mt19937>);

    static_assert(std::same_as<
        BoundFirstRunner::solution_manager_type,
        ComposedSolutionManager>);
    static_assert(std::same_as<
        BoundFirstRunner::neighborhood_explorer_type,
        ComposedNeighborhood>);
    static_assert(!std::copy_constructible<BoundFirstRunner>);
    static_assert(!std::movable<BoundFirstRunner>);
    static_assert(CanRun<BoundFirstRunner>);
    static_assert(!CanRunWithRng<BoundFirstRunner, std::mt19937>);

    using BoundBestRunner = decltype(
        (Runner{BestImprovement{{.max_evaluations = 1}}}
         | default_solution_manager_recipe()
         | default_neighborhood_recipe())
            .bind(std::declval<const AssignmentInstance&>()));

    static_assert(CanRun<BoundBestRunner>);
    static_assert(!CanRunWithRng<BoundBestRunner, std::mt19937>);

    using BoundRandomRunner = decltype(
        (Runner{RandomFirstImprovement{{.max_evaluations = 1}}}
         | default_solution_manager_recipe()
         | default_neighborhood_recipe())
            .bind(std::declval<const AssignmentInstance&>()));

    static_assert(!CanRun<BoundRandomRunner>);
    static_assert(CanRunWithRng<BoundRandomRunner, std::mt19937>);

    return 0;
}
