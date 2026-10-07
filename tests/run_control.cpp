#include "../examples/assignment/cost.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/cost/semantics.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/solvers/multi_start.hpp>
#include <easylocal/solvers/pipeline.hpp>

#include <cassert>
#include <cstddef>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

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
                | easylocal::delta<CapacityCostComponent, ReassignCapacityDelta>())
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<easylocal::runners::BestImprovement>("bi");

    application.runner_parameters<easylocal::runners::FirstImprovement>("fi")
        .max_evaluations = 100;
    application.runner_parameters<easylocal::runners::BestImprovement>("bi")
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

    const auto result =
        easylocal::detail::app_access::run<easylocal::runners::FirstImprovement>(
            application,
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
    const auto best_result =
        easylocal::detail::app_access::run<easylocal::runners::BestImprovement>(
            application,
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
    const auto stopped_result =
        easylocal::detail::app_access::run<easylocal::runners::BestImprovement>(
            application,
            input,
            runtime.solution_manager().initial_solution(),
            easylocal::with(stopped_control));
    assert(stopped_result.evaluations == 1);
    assert(
        stopped_result.termination ==
        easylocal::termination_reason::cancelled);
    assert(runtime.solution_manager().is_valid(stopped_result.solution));

    // The best costs: the first one, then each better one, the last of which
    // is the cost of the result.
    using cost_type = decltype(result.cost);
    std::vector<cost_type> bests;
    auto best_cost_observer = [&](const cost_type& cost) { bests.push_back(cost); };
    easylocal::run_control best_cost_control{};
    best_cost_control.observe_best_cost<cost_type>(best_cost_observer);
    assert(best_cost_control.observes_best_cost<cost_type>());
    assert(!best_cost_control.observes_best_cost<double>());
    const auto descended =
        easylocal::detail::app_access::run<easylocal::runners::FirstImprovement>(
            application,
            input,
            runtime.solution_manager().initial_solution(),
            easylocal::with(best_cost_control));
    assert(!bests.empty());
    assert(
        bests.front()
        == runtime.solution_manager().evaluate(
            runtime.solution_manager().initial_solution()));
    assert(bests.back() == descended.cost);
    for (std::size_t index = 1; index < bests.size(); ++index)
        assert(
            easylocal::cost::better(
                runtime.solution_manager(),
                bests[index],
                bests[index - 1]));
    const auto one_run = bests;

    // An observer of another cost type gets nothing.
    std::size_t other_type_reports = 0;
    auto other_type_observer = [&](const double) { ++other_type_reports; };
    easylocal::run_control other_type_control{};
    other_type_control.observe_best_cost<double>(other_type_observer);
    static_cast<void>(
        easylocal::detail::app_access::run<easylocal::runners::FirstImprovement>(
            application,
            input,
            runtime.solution_manager().initial_solution(),
            easylocal::with(other_type_control)));
    assert(other_type_reports == 0);

    // A solver's runs together: three starts from the same solution, or two
    // stages, report the costs of one run, not one sequence per run.
    auto runner = application.make_runner<easylocal::runners::FirstImprovement>("fi");
    auto restarts = easylocal::make_solver<easylocal::solvers::MultiStart>(
        runner,
        easylocal::solvers::MultiStartParameters{.starts = 3})
                        .initialization(easylocal::initialization::initial);
    std::size_t solve_reports = 0;
    auto counting = [&](const easylocal::run_progress&) { ++solve_reports; };
    easylocal::run_control solve_control{std::stop_token{}, counting};
    solve_control.observe_best_cost<cost_type>(best_cost_observer);
    bests.clear();
    static_cast<void>(restarts.solve(input, easylocal::with(solve_control)));
    assert(bests == one_run);
    assert(solve_reports > 0);
    bests.clear();
    auto stages = easylocal::solvers::pipeline(
        easylocal::solvers::stage("first", runner),
        easylocal::solvers::stage("second", runner));
    static_cast<void>(stages.initialization(easylocal::initialization::initial)
            .solve(input, easylocal::with(solve_control)));
    assert(bests == one_run);

    // shared_best_cost keeps the last cost stored for another thread.
    easylocal::shared_best_cost<cost_type> shared;
    assert(!shared.load());
    std::jthread{[&] { shared.store(one_run.back()); }}.join();
    assert(shared.load() == one_run.back());

    return 0;
}
