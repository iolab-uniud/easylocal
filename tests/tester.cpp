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

    auto application = easylocal::app("assignment")
        .solution_manager(std::move(sm))
        .neighborhood(std::move(nhe))
        .runner<easylocal::runner::first_improvement>("fi");

    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 100;

    return application;
}

using app_type = decltype(make_application());

static_assert(std::copy_constructible<app_type>);
static_assert(std::is_copy_assignable_v<app_type>);
static_assert(std::move_constructible<app_type>);
static_assert(std::is_move_assignable_v<app_type>);
static_assert(std::constructible_from<easylocal::Tester<app_type>, app_type>);
static_assert(std::move_constructible<easylocal::Tester<app_type>>);
static_assert(!std::copy_constructible<easylocal::Tester<app_type>>);
static_assert(std::same_as<
    decltype(std::declval<easylocal::Tester<app_type>&>().input()),
    const AssignmentInstance&>);


[[nodiscard]]
auto make_input(const quantity_type demand) -> AssignmentInstance
{
    return AssignmentInstance{
        .demand = {demand, demand + 1},
        .capacity = {10, 20},
    };
}

void tester_owns_input_and_builds_instance_from_it()
{
    easylocal::Tester tester{make_application()};

    assert(!tester.has_input());

    tester.set_input(make_input(3));

    assert(tester.has_input());
    assert(tester.input().demand[0] == 3);
    assert(std::addressof(tester.instance().instance()) == std::addressof(tester.input()));
}

void replacing_input_rebuilds_the_app_instance()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(3));

    tester.set_input(make_input(7));

    assert(tester.input().demand[0] == 7);
    assert(tester.instance().instance().demand[0] == 7);
    assert(std::addressof(tester.instance().instance()) == std::addressof(tester.input()));
}

void moving_tester_preserves_instance_input_binding()
{
    easylocal::Tester tester{make_application()};
    tester.set_input(make_input(11));

    auto moved = std::move(tester);

    assert(moved.has_input());
    assert(moved.input().demand[0] == 11);
    assert(std::addressof(moved.instance().instance()) == std::addressof(moved.input()));
}

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
    tester_owns_input_and_builds_instance_from_it();
    replacing_input_rebuilds_the_app_instance();
    moving_tester_preserves_instance_input_binding();
}
