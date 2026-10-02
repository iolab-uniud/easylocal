#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <future>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{

using namespace easylocal::mwe::assignment;

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
    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::mwe::assignment::assignment_cost();

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
        application.for_input(input);
    };

template<class App>
concept can_materialize_from_rvalue_input =
    requires(const App& application, typename App::input_type&& input) {
        application.for_input(std::move(input));
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
concept has_legacy_instance_accessor = requires(const Runtime& runtime) {
    runtime.instance();
};

using AssignmentRuntime = decltype(
    std::declval<const AssignmentApp&>().for_input(
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
    auto runtime = application.for_input(instance);

    assert(&runtime.input() == &instance);
    assert(&runtime.solution_manager().input() == &instance);
    assert(&runtime.neighborhood().input() == &instance);

    auto fi = runtime.runner<easylocal::runners::FirstImprovement>();
    auto bi = runtime.runner<easylocal::runners::BestImprovement>();

    assert(&fi.solution_manager() == &runtime.solution_manager());
    assert(&bi.solution_manager() == &runtime.solution_manager());
    assert(&fi.neighborhood_explorer() == &runtime.neighborhood());
    assert(&bi.neighborhood_explorer() == &runtime.neighborhood());
}

void registered_runners_are_executable()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto application = make_application();
    auto runtime = application.for_input(instance);
    const auto initial = runtime.solution_manager().initial_solution();

    const auto fi = runtime.run<easylocal::runners::FirstImprovement>(initial);
    assert(runtime.solution_manager().is_valid(fi.solution));

    const auto bi = runtime.run<easylocal::runners::BestImprovement>(initial);
    assert(runtime.solution_manager().is_valid(bi.solution));
}

void direct_app_runs_use_fresh_runtime_state()
{
    const AssignmentInstance instance{
        .demand = {4, 4, 2},
        .capacity = {5, 5},
    };

    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::mwe::assignment::assignment_cost();

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

    auto seed_runtime = application.for_input(instance);
    const auto initial = seed_runtime.solution_manager().initial_solution();

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

    auto shared_runtime = application.for_input(instance);
    const auto shared_first = shared_runtime.run<StatefulRunner>(initial);
    const auto shared_second = shared_runtime.run<StatefulRunner>(initial);
    assert(shared_first.invocation == 1);
    assert(shared_second.invocation == 2);
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

    auto runtime = application.for_input(instance);
    assert(runtime.solution_manager().is_valid(result.solution));
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
    auto runtime = application.for_input(instance);
    assert(runtime.solution_manager().is_valid(result.solution));
}

void named_runner_registrations_can_be_selected_for_solver_creation()
{
    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::mwe::assignment::assignment_cost();

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
    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::mwe::assignment::assignment_cost();
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
    direct_app_runs_use_fresh_runtime_state();
    app_can_materialize_standard_runners();
    app_can_make_and_equip_solvers();
    named_runner_registrations_can_be_selected_for_solver_creation();
}
