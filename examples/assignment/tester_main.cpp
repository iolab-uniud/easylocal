#include "capacity_delta.hpp"
#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "demo_runner.hpp"

#include <easylocal/app.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/tester.hpp>
#include <easylocal/tui/tester.hpp>

#ifndef EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE
#error "EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE must name the example instance"
#endif

int main()
{
    using namespace easylocal::mwe::assignment;

    auto application = easylocal::app("assignment-tester")
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
        .runner<easylocal::search::FirstImprovement>("fi")
        .runner<demo::SlowFirstImprovement>("slow-fi");

    application
        .runner_config<easylocal::search::FirstImprovement>()
        .max_evaluations = 100;

    auto& slow_config =
        application.runner_config<demo::SlowFirstImprovement>();
    slow_config.max_evaluations = 2000;
    slow_config.delay_ms = 5;

    easylocal::Tester tester{std::move(application)};
    tester.set_input(load_instance(EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE));

    easylocal::tui::run(
        tester,
        {
            .title = "EasyLocal++ Assignment Tester",
            .seed = 0,
            .input_path = EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE,
        });
}
