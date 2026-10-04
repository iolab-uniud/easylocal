#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers/pipeline.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <future>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace assignment;

struct StatefulRunnerConfig
{
};

template<class Solution>
struct StatefulRunnerResult
{
    Solution solution;
    int invocation{};
};

class StatefulRunner
{
public:
    using parameters_type = StatefulRunnerConfig;

    explicit StatefulRunner(StatefulRunnerConfig) noexcept
    {
    }

    template<class Context>
    [[nodiscard]] auto run(
        const Context&,
        typename Context::solution_type solution)
    {
        return StatefulRunnerResult<typename Context::solution_type>{
            .solution = std::move(solution),
            .invocation = ++invocations_,
        };
    }

private:
    int invocations_{};
};

[[nodiscard]]
auto make_application()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(std::move(nhe))
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<easylocal::runners::BestImprovement>("bi");

    application
        .runner_config<easylocal::runners::FirstImprovement>()
        .max_evaluations = 100;
    application
        .runner_config<easylocal::runners::BestImprovement>()
        .max_evaluations = 100;

    return application;
}

template<class App>
concept can_materialize_from_lvalue_input =
    requires(const App& application, typename App::input_type& input) {
        application.bind(input);
    };

template<class App>
concept can_materialize_from_rvalue_input =
    requires(const App& application, typename App::input_type&& input) {
        application.bind(std::move(input));
    };

template<class RunnerType>
concept can_bind_lvalue_input =
    requires(const RunnerType& runner, typename RunnerType::input_type& input) {
        runner.bind(input);
    };

template<class RunnerType>
concept can_bind_rvalue_input =
    requires(const RunnerType& runner, typename RunnerType::input_type&& input) {
        runner.bind(std::move(input));
    };

using AssignmentApp = decltype(make_application());
using AssignmentRunner = decltype(
    std::declval<const AssignmentApp&>()
        .template make_runner<easylocal::runners::FirstImprovement>());

template<class Runtime>
concept has_legacy_instance_type = requires {
    typename Runtime::instance_type;
};

template<class Runtime>
concept has_legacy_instance_accessor = requires(const Runtime& bound_app) {
    bound_app.instance();
};

using AssignmentRuntime = decltype(std::declval<const AssignmentApp&>().bind(
    std::declval<const AssignmentApp::input_type&>()));

static_assert(!has_legacy_instance_type<AssignmentRuntime>);
static_assert(!has_legacy_instance_accessor<AssignmentRuntime>);

static_assert(can_materialize_from_lvalue_input<AssignmentApp>);
static_assert(!can_materialize_from_rvalue_input<AssignmentApp>);
static_assert(can_bind_lvalue_input<AssignmentRunner>);
static_assert(!can_bind_rvalue_input<AssignmentRunner>);

void app_owns_runner_configuration_and_names()
{
    auto application = make_application();

    assert(application.name() == "assignment");
    assert(
        application.runner_name<easylocal::runners::FirstImprovement>() ==
        std::string_view{"fi"});
    assert(
        application.runner_name<easylocal::runners::BestImprovement>() ==
        std::string_view{"bi"});
    assert(
        application
            .runner_config<easylocal::runners::FirstImprovement>()
            .max_evaluations == 100);
}

void one_input_materializes_one_shared_graph_for_all_runners()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto application = make_application();
    auto bound_app = application.bind(instance);

    assert(&bound_app.input() == &instance);
    assert(&bound_app.solution_manager().input() == &instance);
    assert(&bound_app.neighborhood().input() == &instance);

    auto fi = bound_app.runner<easylocal::runners::FirstImprovement>();
    auto bi = bound_app.runner<easylocal::runners::BestImprovement>();

    assert(&fi.solution_manager() == &bound_app.solution_manager());
    assert(&bi.solution_manager() == &bound_app.solution_manager());
    assert(&fi.neighborhood_explorer() == &bound_app.neighborhood());
    assert(&bi.neighborhood_explorer() == &bound_app.neighborhood());
}

void registered_runners_are_executable()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto application = make_application();
    auto bound_app = application.bind(instance);
    const auto initial = bound_app.solution_manager().initial_solution();

    const auto fi = bound_app.run<easylocal::runners::FirstImprovement>(initial);
    assert(bound_app.solution_manager().is_valid(fi.solution));

    const auto bi = bound_app.run<easylocal::runners::BestImprovement>(initial);
    assert(bound_app.solution_manager().is_valid(bi.solution));
}

void direct_app_runs_use_fresh_bound_app_state()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("stateful")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(std::move(nhe))
            .with_runner<StatefulRunner>("stateful");

    auto seed_bound_app = application.bind(instance);
    const auto initial = seed_bound_app.solution_manager().initial_solution();

    const auto first = application.run<StatefulRunner>(instance, initial);
    const auto second = application.run<StatefulRunner>(instance, initial);
    assert(first.invocation == 1);
    assert(second.invocation == 1);

    auto concurrent_first = std::async(
        std::launch::async,
        [&] { return application.run<StatefulRunner>(instance, initial); });
    auto concurrent_second = std::async(
        std::launch::async,
        [&] { return application.run<StatefulRunner>(instance, initial); });
    assert(concurrent_first.get().invocation == 1);
    assert(concurrent_second.get().invocation == 1);

    auto shared_bound_app = application.bind(instance);
    const auto shared_first = shared_bound_app.run<StatefulRunner>(initial);
    const auto shared_second = shared_bound_app.run<StatefulRunner>(initial);
    assert(shared_first.invocation == 1);
    assert(shared_second.invocation == 2);
}

void registered_runners_can_be_run_by_name()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    const auto application = make_application();
    const auto initial = application.bind(instance).solution_manager().initial_solution();
    std::mt19937_64 rng{1};

    // The same runner, chosen by name or by algorithm, gives the same search.
    const auto by_name = application.run("bi", instance, initial, rng);
    assert(by_name);
    const auto by_algorithm =
        application.run<easylocal::runners::BestImprovement>(instance, initial);
    assert(by_name->solution == by_algorithm.solution);

    // No runner has that name: nothing runs.
    assert(!application.run("missing", instance, initial, rng));

    // Options reach the runner: a run stopped before it starts keeps the
    // initial solution.
    std::stop_source stop;
    stop.request_stop();
    const easylocal::run_control control{stop.get_token()};
    const auto stopped =
        application.run("fi", instance, initial, rng, easylocal::with(control));
    assert(stopped);
    assert(stopped->solution == initial);
}

void pipelines_are_registered_and_run_by_name()
{
    namespace solvers = easylocal::solvers;
    using easylocal::runners::BestImprovement;
    using easylocal::runners::FirstImprovement;
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();
    auto nhe = easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();
    auto descent =
        easylocal::make_runner<FirstImprovement>({.max_evaluations = 100}) | sm | nhe;
    auto best =
        easylocal::make_runner<BestImprovement>({.max_evaluations = 100}) | sm | nhe;
    const auto stages = [&] {
        return solvers::pipeline(
            solvers::stage("feasible", descent) & solvers::until_feasible()
                & solvers::attempts(3),
            solvers::stage("best", best));
    };

    auto application = easylocal::app("assignment") | sm | nhe
        | easylocal::runner<FirstImprovement>("fi")
        | easylocal::pipeline(
            "cascade",
            solvers::stage("feasible", descent) & solvers::until_feasible()
                & solvers::attempts(3),
            solvers::stage("best", best))
        | easylocal::runner<BestImprovement>("bi");

    // The names of every registration, in order; the runners alone without the
    // pipeline, which keeps the runners' lookup by algorithm.
    std::vector<std::string_view> names;
    application.for_each_registration_name([&](const std::string_view name) {
        names.push_back(name);
    });
    assert((names == std::vector<std::string_view>{"fi", "cascade", "bi"}));
    std::size_t runners = 0;
    application.for_each_runner_registration(
        [&]<class Algorithm>(
            std::string_view,
            const typename Algorithm::parameters_type&) { ++runners; });
    assert(runners == 2);
    application.runner_config<BestImprovement>().max_evaluations = 100;

    // Run by name from a solution, as the pipeline runs it.
    const auto initial = application.bind(instance).solution_manager().initial_solution();
    std::mt19937_64 rng{3};
    const auto by_name = application.run("cascade", instance, initial, rng);
    assert(by_name && by_name->effort);
    std::mt19937_64 same_rng{3};
    const auto direct = stages().run(instance, initial, same_rng);
    assert(by_name->solution == direct.solution);
    assert(by_name->effort->evaluations == direct.evaluations);

    // The stages' parameters under runners.<name>, also in the read-only set.
    const auto has =
        [](const easylocal::config::parameter_set& parameters, std::string_view path) {
            for (const auto& parameter : parameters.parameters())
                if (parameter.path == path)
                    return true;
            return false;
        };
    const auto parameters = application.configuration();
    assert(has(parameters, "runners.cascade.feasible.attempts"));
    assert(has(parameters, "runners.cascade.best.search.max_evaluations"));
    assert(has(
        std::as_const(application).configuration(),
        "runners.cascade.feasible.attempts"));
    const auto overridden = easylocal::config::apply_overrides(
        parameters,
        std::vector<easylocal::config::text_override>{
            {"runners.cascade.best.search.max_evaluations", "5"}});
    assert(overridden);

    // A session lists and runs the pipeline as a runner.
    easylocal::Session session{application, instance, 7};
    session.use_initial_solution();
    const auto session_names = session.runner_names();
    assert(session_names.size() == 3 && session_names[1] == "cascade");
    assert(session.run("cascade"));
    assert(session.last_run_effort().has_value());

    // An app with a pipeline only.
    auto only = easylocal::app("only") | sm | nhe
        | easylocal::pipeline(
            "two",
            (solvers::stage("feasible", descent) & solvers::until_feasible())
                | solvers::stage("best", best));
    assert(only.run("two", instance, initial, rng));
}

void app_can_materialize_standard_runners()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto application = make_application();
    auto runner = application.make_runner<easylocal::runners::FirstImprovement>();
    auto bound = runner.bind(instance);
    assert(&bound.input() == &instance);
    const auto initial = bound.initial_solution();
    const auto result = bound.run(initial);

    auto bound_app = application.bind(instance);
    assert(bound_app.solution_manager().is_valid(result.solution));
}

void app_can_make_and_equip_solvers()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto application = make_application();
    auto solver = application.make_solver<
        easylocal::solvers::LocalSearch,
        easylocal::runners::FirstImprovement>(
        easylocal::solvers::LocalSearchConfig<easylocal::initialization::Initial>{
            .initialization = easylocal::initialization::initial,
            .seed = 17,
        });

    const auto result = solver.solve(instance);
    auto bound_app = application.bind(instance);
    assert(bound_app.solution_manager().is_valid(result.solution));
}

void named_runner_registrations_can_be_selected_for_solver_creation()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(std::move(nhe))
            .with_runner<easylocal::runners::FirstImprovement>("quick")
            .with_runner<easylocal::runners::FirstImprovement>("deep");

    application
        .runner_config<easylocal::runners::FirstImprovement>("quick")
        .max_evaluations = 1;
    application
        .runner_config<easylocal::runners::FirstImprovement>("deep")
        .max_evaluations = 100;

    const auto quick_runner =
        application.make_runner<easylocal::runners::FirstImprovement>("quick");
    const auto deep_runner =
        application.make_runner<easylocal::runners::FirstImprovement>("deep");

    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto quick_bound = quick_runner.bind(instance);
    auto deep_bound = deep_runner.bind(instance);
    const auto quick_result = quick_bound.run(quick_bound.initial_solution());
    const auto deep_result = deep_bound.run(deep_bound.initial_solution());

    assert(quick_result.evaluations <= 1);
    assert(deep_result.evaluations >= quick_result.evaluations);

    auto solver = application.make_solver<
        easylocal::solvers::LocalSearch,
        easylocal::runners::FirstImprovement>(
        "deep",
        easylocal::solvers::LocalSearchConfig<easylocal::initialization::Initial>{
            .initialization = easylocal::initialization::initial,
            .seed = 23,
        });
    const auto solver_result = solver.solve(instance);
    assert(solver_result.evaluations == deep_result.evaluations);

    bool rejected_unknown_name = false;
    try
    {
        [[maybe_unused]] auto missing =
            application.make_runner<easylocal::runners::FirstImprovement>(
                "missing");
    }
    catch (const std::invalid_argument&)
    {
        rejected_unknown_name = true;
    }
    assert(rejected_unknown_name);
}

} // namespace

void app_builder_pipes_and_registration_parameters()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();
    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    const auto fluent =
        easylocal::app("fluent")
            .with_solution_manager(sm)
            .with_neighborhood(nhe)
            .with_runner<easylocal::runners::FirstImprovement>(
                "fi", {.max_evaluations = 5});
    const auto piped =
        easylocal::app("piped")
        | sm
        | nhe
        | easylocal::runner<easylocal::runners::FirstImprovement>(
              "fi", {.max_evaluations = 5});

    static_assert(std::same_as<decltype(fluent), decltype(piped)>);
    assert(fluent.runner_config<easylocal::runners::FirstImprovement>()
               .max_evaluations == 5);
    assert(piped.runner_config<easylocal::runners::FirstImprovement>()
               .max_evaluations == 5);
    assert(piped.runner_name<easylocal::runners::FirstImprovement>() == "fi");
}

int main()
{
    app_builder_pipes_and_registration_parameters();
    app_owns_runner_configuration_and_names();
    one_input_materializes_one_shared_graph_for_all_runners();
    registered_runners_are_executable();
    direct_app_runs_use_fresh_bound_app_state();
    registered_runners_can_be_run_by_name();
    pipelines_are_registered_and_run_by_name();
    app_can_materialize_standard_runners();
    app_can_make_and_equip_solvers();
    named_runner_registrations_can_be_selected_for_solver_creation();
}
