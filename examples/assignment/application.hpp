#pragma once

// The app of the assignment problem, shared by the command-line program, the
// TextUI and the REST service.

#include "cost.hpp"
#include "demo_runner.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <string>

namespace assignment
{

// Two runners: First Improvement, which stops at a local optimum, and a slow
// one to watch while it runs. The type of an app spells out all its recipes,
// so auto deduces it.
inline auto make_application(const std::string& name)
{
    return easylocal::app(name)
        | (easylocal::solution_manager<AssignmentSolutionManager>() | assignment_cost())
        | easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi")
        | easylocal::runner<demo::SlowFirstImprovement>(
            "slow-fi",
            {.max_evaluations = 2000, .delay_ms = 5});
}

} // namespace assignment
