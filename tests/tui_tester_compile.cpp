#include "../examples/assignment/cost.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/demo_runner.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/adapters/tui/tester.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/solvers/pipeline.hpp>

int main()
{
    using namespace assignment;

    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();
    auto nhe = easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();
    auto descent =
        easylocal::make_runner<easylocal::runners::FirstImprovement>({}) | sm | nhe;

    // Two runners and a pipeline: the tester lists and runs them alike.
    auto application =
        easylocal::app("tui-compile")
            .with_solution_manager(sm)
            .with_neighborhood(nhe)
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<demo::SlowFirstImprovement>("slow-fi")
            .with_pipeline(
                easylocal::pipeline(
                    "cascade",
                    (easylocal::solvers::stage("feasible", descent)
                        & easylocal::solvers::until_feasible())
                        | easylocal::solvers::stage("descent", descent)));

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
