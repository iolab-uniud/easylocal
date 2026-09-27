#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/app.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/tester.hpp>

#include <cassert>
#include <concepts>
#include <string_view>
#include <type_traits>
#include <utility>

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

    return easylocal::app("assignment")
        .solution_manager(std::move(sm))
        .neighborhood(std::move(nhe))
        .runner<easylocal::runner::first_improvement>("fi");
}

using app_type = decltype(make_application());

static_assert(std::copy_constructible<app_type>);
static_assert(std::is_copy_assignable_v<app_type>);
static_assert(std::move_constructible<app_type>);
static_assert(std::is_move_assignable_v<app_type>);
static_assert(std::constructible_from<easylocal::Tester<app_type>, app_type>);

void app_copy_preserves_graph_configuration()
{
    auto application = make_application();
    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 17;

    const auto copy = application;

    assert(copy.name() == std::string_view{"assignment"});
    assert(
        copy.runner_name<easylocal::runner::first_improvement>() ==
        std::string_view{"fi"});
    assert(
        copy.runner_config<easylocal::runner::first_improvement>()
            .max_evaluations == 17);
}

void tester_can_copy_an_lvalue_app()
{
    auto application = make_application();
    easylocal::Tester tester{application};

    assert(tester.app().name() == std::string_view{"assignment"});
    assert(application.name() == std::string_view{"assignment"});

    tester
        .app()
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 3;

    assert(
        application
            .runner_config<easylocal::runner::first_improvement>()
            .max_evaluations != 3);
}

void tester_can_take_ownership_of_an_rvalue_app()
{
    auto application = make_application();
    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 29;

    easylocal::Tester tester{std::move(application)};

    assert(tester.app().name() == std::string_view{"assignment"});
    assert(
        tester
            .app()
            .runner_config<easylocal::runner::first_improvement>()
            .max_evaluations == 29);
}

} // namespace

int main()
{
    app_copy_preserves_graph_configuration();
    tester_can_copy_an_lvalue_app();
    tester_can_take_ownership_of_an_rvalue_app();
}
