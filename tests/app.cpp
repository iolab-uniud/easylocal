#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/app.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>

#include <cassert>
#include <cstddef>
#include <string_view>

namespace
{

using namespace easylocal::mwe::assignment;

[[nodiscard]]
auto make_application()
{
    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::component<CapacityCostComponent>()
        | easylocal::component<LoadImbalanceCostComponent>()
        | easylocal::aggregator(AssignmentCostAggregator{});

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment")
            .solution_manager(std::move(sm))
            .neighborhood(std::move(nhe))
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

void app_owns_runner_configuration_and_names()
{
    auto application = make_application();

    assert(application.name() == "assignment");
    assert(
        application.runner_name<easylocal::runner::first_improvement>() ==
        std::string_view{"fi"});
    assert(
        application.runner_name<easylocal::runner::best_improvement>() ==
        std::string_view{"bi"});
    assert(
        application
            .runner_config<easylocal::runner::first_improvement>()
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

    assert(&runtime.instance() == &instance);
    assert(&runtime.solution_manager().instance() == &instance);
    assert(&runtime.neighborhood().instance() == &instance);

    auto fi = runtime.runner<easylocal::runner::first_improvement>();
    auto bi = runtime.runner<easylocal::runner::best_improvement>();

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

    const auto fi = runtime.run<easylocal::runner::first_improvement>(initial);
    assert(runtime.solution_manager().is_valid(fi.solution));

    const auto bi = runtime.run<easylocal::runner::best_improvement>(initial);
    assert(runtime.solution_manager().is_valid(bi.solution));
}

} // namespace

int main()
{
    app_owns_runner_configuration_and_names();
    one_input_materializes_one_shared_graph_for_all_runners();
    registered_runners_are_executable();
}
