#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/config/tree.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>

#include <array>
#include <cassert>
#include <concepts>
#include <random>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{

using namespace assignment;
using easylocal::runners::BestImprovement;
using easylocal::runners::FirstImprovement;

[[nodiscard]]
auto default_solution_manager_recipe()
{
    return easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::cost::apply(
            assignment::CapacityHardCost{},
            easylocal::component<CapacityCostComponent>());
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

template<class Path, std::size_t Size>
[[nodiscard]]
consteval auto path_is(const std::array<std::string_view, Size>& expected)
    -> bool
{
    constexpr auto actual = Path::segments();
    if constexpr (actual.size() != Size)
    {
        return false;
    }
    else
    {
        return actual == expected;
    }
}

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

    auto configured =
        Runner{FirstImprovement{{.max_evaluations = 17}}}
        | default_solution_manager_recipe()
        | default_neighborhood_recipe();

    const auto configuration = easylocal::config::root(
        configured.template configuration<"solver">());

    bool saw_search_budget = false;
    easylocal::config::for_each_config_parameter(
        configuration,
        [&](const auto path, const auto, const auto& value) {
            using path_type = std::remove_cvref_t<decltype(path)>;
            if constexpr (path_is<path_type>(
                              std::array<std::string_view, 3>{
                                  "solver",
                                  "search",
                                  "max_evaluations"}))
            {
                saw_search_budget = value == 17;
            }
        });

    assert(saw_search_budget);

    const auto& search_endpoint =
        easylocal::config::at<"solver", "search">(configuration);
    auto updated_search = search_endpoint.parameters();
    updated_search.max_evaluations = 2;
    assert(search_endpoint.configure(updated_search));

    const AssignmentInstance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };
    const AssignmentSolution initial{
        .assignment = {0, 0, 1},
    };
    const auto configured_result = configured.bind(instance).run(initial);
    assert(configured_result.evaluations == 2);
    assert(
        configured_result.termination ==
        easylocal::termination_reason::
            evaluation_budget_exhausted);


    return 0;
}
