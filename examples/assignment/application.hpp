#pragma once

#include "cost.hpp"
#include "demo_runner.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <string>

namespace assignment
{

// The app shared by the terminal tester and the REST service: two runners,
// a fast First Improvement and a slowed-down one to watch while it runs. The
// type of an app spells out all its recipes, so auto deduces it.
inline auto make_application(const std::string& name)
{
    auto application = easylocal::app(name)
        | (easylocal::solution_manager<AssignmentSolutionManager>() | assignment_cost())
        | easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi")
        | easylocal::runner<demo::SlowFirstImprovement>("slow-fi");

    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        100;

    auto& slow_config = application.runner_config<demo::SlowFirstImprovement>();
    slow_config.max_evaluations = 2000;
    slow_config.delay_ms = 5;
    return application;
}

} // namespace assignment
