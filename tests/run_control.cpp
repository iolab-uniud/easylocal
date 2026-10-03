#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>

#include <cassert>
#include <cstddef>
#include <stop_token>
#include <utility>

namespace
{

using namespace assignment;

[[nodiscard]] auto make_application()
{
    auto application =
        easylocal::app("controlled")
            .with_solution_manager(
                easylocal::solution_manager<AssignmentSolutionManager>()
                | assignment::assignment_cost())
            .with_neighborhood(
                easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
                | easylocal::delta<
                    CapacityCostComponent,
                    ReassignCapacityDeltaEvaluator>())
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<easylocal::runners::BestImprovement>("bi");

    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        100;
    application.runner_config<easylocal::runners::BestImprovement>().max_evaluations =
        100;
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

    auto runtime = application.bind(input);
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

    const auto result = application.run<easylocal::runners::FirstImprovement>(
        input,
        std::move(initial),
        easylocal::with(control));

    assert(observations >= 3);
    assert(last_evaluations == result.evaluations);
    assert(result.evaluations == 3);
    assert(
        result.termination ==
        easylocal::termination_reason::cancelled);

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
    const auto best_result = application.run_at<1>(
        input,
        std::move(best_initial),
        easylocal::with(best_control));
    assert(best_observations >= 3);
    assert(best_result.evaluations == 3);
    assert(
        best_result.termination ==
        easylocal::termination_reason::cancelled);

    // Every run is cancellable: a stop requested before the run starts ends
    // it right after the initial evaluation.
    std::stop_source already_stopped;
    already_stopped.request_stop();
    const easylocal::run_control stopped_control{already_stopped.get_token()};
    const auto stopped_result = application.run_at<1>(
        input,
        runtime.solution_manager().initial_solution(),
        easylocal::with(stopped_control));
    assert(stopped_result.evaluations == 1);
    assert(
        stopped_result.termination ==
        easylocal::termination_reason::cancelled);
    assert(runtime.solution_manager().is_valid(stopped_result.solution));

    return 0;
}
