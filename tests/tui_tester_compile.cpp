#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/demo_runner.hpp"
#include "../examples/assignment/instance_io.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/adapters/tui/tester.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/runners/first_improvement.hpp>

int main()
{
    using namespace assignment;

    auto application =
        easylocal::app("tui-compile")
            .with_solution_manager(
                easylocal::solution_manager<AssignmentSolutionManager>()
                | assignment::assignment_cost())
            .with_neighborhood(
                easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
                | easylocal::delta<
                    CapacityCostComponent,
                    ReassignCapacityDeltaEvaluator>())
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<demo::SlowFirstImprovement>("slow-fi");

    using session_type = easylocal::Session<decltype(application)>;
    static_assert(session_type::supports_input_loading);
    static_assert(session_type::supports_solution_loading);
    static_assert(session_type::supports_solution_saving);

    // Instantiates the complete FTXUI frontend without entering a terminal loop.
    if (false)
    {
        easylocal::tui::run(std::move(application));
    }

    return 0;
}
