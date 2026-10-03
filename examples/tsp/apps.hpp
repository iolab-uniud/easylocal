#pragma once

#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/first_improvement.hpp>

namespace tsp
{

// Two apps over the same SolutionManager, one per neighborhood. The type of an
// app spells out all its recipes, so the functions let auto deduce it.
inline auto two_opt_app()
{
    auto application = easylocal::app("tsp-two-opt")
        | (easylocal::solution_manager<TspSolutionManager>()
            | easylocal::cost::apply(
                TourLengthCost{},
                easylocal::component<TourLengthComponent>()))
        | (easylocal::neighborhood<TwoOptNeighborhoodExplorer>()
            | easylocal::delta<TourLengthComponent, TwoOptTourLengthDeltaEvaluator>())
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi");
    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        100;
    return application;
}

inline auto swap_app()
{
    auto application = easylocal::app("tsp-swap")
        | (easylocal::solution_manager<TspSolutionManager>()
            | easylocal::cost::apply(
                TourLengthCost{},
                easylocal::component<TourLengthComponent>()))
        | (easylocal::neighborhood<SwapCitiesNeighborhoodExplorer>()
            | easylocal::delta<TourLengthComponent, SwapTourLengthDeltaEvaluator>())
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi");
    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        100;
    return application;
}

} // namespace tsp
