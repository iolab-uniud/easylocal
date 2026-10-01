#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance_io.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "../examples/assignment/demo_runner.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/app/tester.hpp>
#include <easylocal/adapters/tui/tester.hpp>

int main()
{
    using namespace easylocal::mwe::assignment;

    auto application = easylocal::app("tui-compile")
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
        .runner<easylocal::runners::FirstImprovement>("fi")
        .runner<demo::SlowFirstImprovement>("slow-fi");

    easylocal::Tester tester{std::move(application)};
    static_assert(decltype(tester)::supports_input_loading);
    static_assert(decltype(tester)::supports_solution_loading);
    static_assert(decltype(tester)::supports_solution_saving);

    // Instantiates the complete FTXUI frontend without entering a terminal loop.
    if (false)
    {
        easylocal::tui::run(tester);
    }

    return 0;
}
