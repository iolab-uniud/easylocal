#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/app.hpp>
#include <easylocal/run_control.hpp>
#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>

#include <cassert>
#include <cstddef>
#include <stop_token>
#include <utility>

namespace
{

using namespace easylocal::mwe::assignment;


struct PlainRunnerConfig
{
};

template<class Solution>
struct PlainRunnerResult
{
    Solution solution;
};

class PlainRunner
{
public:
    explicit PlainRunner(PlainRunnerConfig) noexcept {}

    template<class Context>
    [[nodiscard]] auto run(
        const Context&,
        typename Context::solution_type solution) const
    {
        return PlainRunnerResult<typename Context::solution_type>{
            .solution = std::move(solution),
        };
    }
};

using plain_runner = easylocal::runner::algorithm_tag<
    PlainRunner,
    PlainRunnerConfig>;

[[nodiscard]] auto make_application()
{
    auto application = easylocal::app("controlled")
        .solution_manager(
            easylocal::solution_manager<AssignmentSolutionManager>()
            | easylocal::component<CapacityCostComponent>()
            | easylocal::component<LoadImbalanceCostComponent>()
            | easylocal::aggregator(AssignmentCostAggregator{}))
        .neighborhood(
            easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
            | easylocal::delta<
                  CapacityCostComponent,
                  ReassignCapacityDeltaEvaluator>())
        .runner<easylocal::runner::first_improvement>("fi")
        .runner<easylocal::runner::best_improvement>("bi");

    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 100;
    application
        .runner_config<easylocal::runner::best_improvement>()
        .max_evaluations = 100;
    return application;
}

} // namespace

int main()
{
    const AssignmentInstance input{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto application = make_application();
    static_assert(decltype(application)::template runner_supports_run_control<0>);
    static_assert(decltype(application)::template runner_supports_run_control<1>);

    auto runtime = application.for_input(input);
    auto initial = runtime.solution_manager().initial_solution();

    std::stop_source stop;
    std::size_t observations = 0;
    std::size_t last_evaluations = 0;
    auto observer = [&](const easylocal::run_progress& progress) {
        ++observations;
        last_evaluations = progress.evaluations;
        assert(progress.evaluation_limit == 100);
        if (progress.evaluations >= 3)
        {
            stop.request_stop();
        }
    };
    const easylocal::run_control control{stop.get_token(), observer};

    const auto result = application.run_controlled<
        easylocal::runner::first_improvement>(
            input,
            std::move(initial),
            control);

    assert(observations >= 3);
    assert(last_evaluations == result.evaluations);
    assert(result.evaluations == 3);
    assert(
        result.termination ==
        easylocal::search::FirstImprovementTermination::cancelled);

    auto best_initial = runtime.solution_manager().initial_solution();
    std::stop_source best_stop;
    std::size_t best_observations = 0;
    auto best_observer = [&](const easylocal::run_progress& progress) {
        ++best_observations;
        if (progress.evaluations >= 3)
        {
            best_stop.request_stop();
        }
    };
    const easylocal::run_control best_control{
        best_stop.get_token(),
        best_observer};
    const auto best_result = application.run_controlled_at<1>(
        input,
        std::move(best_initial),
        best_control);
    assert(best_observations >= 3);
    assert(best_result.evaluations == 3);
    assert(
        best_result.termination ==
        easylocal::search::BestImprovementTermination::cancelled);

    auto plain_application = easylocal::app("plain")
        .solution_manager(
            easylocal::solution_manager<AssignmentSolutionManager>()
            | easylocal::component<CapacityCostComponent>()
            | easylocal::component<LoadImbalanceCostComponent>()
            | easylocal::aggregator(AssignmentCostAggregator{}))
        .neighborhood(
            easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
            | easylocal::delta<
                  CapacityCostComponent,
                  ReassignCapacityDeltaEvaluator>())
        .runner<plain_runner>("plain");

    static_assert(
        !decltype(plain_application)::template runner_supports_run_control<0>);
    auto plain_runtime = plain_application.for_input(input);
    auto plain_initial = plain_runtime.solution_manager().initial_solution();
    std::stop_source already_stopped;
    already_stopped.request_stop();
    const easylocal::run_control ignored_control{already_stopped.get_token()};
    const auto plain_result = plain_application.run_controlled_at<0>(
        input,
        std::move(plain_initial),
        ignored_control);
    assert(plain_runtime.solution_manager().is_valid(plain_result.solution));

    return 0;
}
