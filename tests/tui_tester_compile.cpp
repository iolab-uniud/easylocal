#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/app.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/tester.hpp>
#include <easylocal/tui/tester.hpp>

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
        .runner<easylocal::runner::first_improvement>("fi");

    easylocal::Tester tester{std::move(application)};

    // Instantiates the complete FTXUI frontend without entering a terminal loop.
    if (false)
    {
        easylocal::tui::run(tester);
    }

    return 0;
}
