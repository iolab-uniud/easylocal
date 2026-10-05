#pragma once

// The apps of the TSP example over the same SolutionManager and cost: one per
// neighborhood (2-opt and swap), for the launcher of tui_main.cpp, and one
// whose runners use both.

#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/runners/first_improvement.hpp>

namespace tsp
{

// [apps] -------------------------------------------------------------------
// The SolutionManager recipe of both apps: the launcher of tui_main.cpp
// passes the Input and the solution from one app to the other, so they must
// have the same one.
inline auto tsp_solution_manager()
{
    return easylocal::solution_manager<TspSolutionManager>()
        | easylocal::component<TourLengthComponent>();
}

// Two apps over the same SolutionManager, one per neighborhood. The type of an
// app spells out all its recipes, so the functions let auto deduce it.
inline auto two_opt_app()
{
    return easylocal::app("tsp-two-opt") | tsp_solution_manager()
        | (easylocal::neighborhood<TwoOptNeighborhoodExplorer>()
            | easylocal::delta<TourLengthComponent, TwoOptTourLengthDelta>())
        | easylocal::runner<easylocal::runners::FirstImprovement>(
            "fi",
            {.max_evaluations = 100});
}

inline auto swap_app()
{
    return easylocal::app("tsp-swap") | tsp_solution_manager()
        | (easylocal::neighborhood<SwapCitiesNeighborhoodExplorer>()
            | easylocal::delta<TourLengthComponent, SwapTourLengthDelta>())
        | easylocal::runner<easylocal::runners::FirstImprovement>(
            "fi",
            {.max_evaluations = 100});
}
// [apps] -------------------------------------------------------------------

// [runner-neighborhood] ----------------------------------------------------
// One app with both neighborhoods: its own is 2-opt, the runner "fi" uses it,
// and the runner "fi-swap" brings its own, built over the same
// SolutionManager.
inline auto tsp_app()
{
    return easylocal::app("tsp") | tsp_solution_manager()
        | (easylocal::neighborhood<TwoOptNeighborhoodExplorer>()
            | easylocal::delta<TourLengthComponent, TwoOptTourLengthDelta>())
        | easylocal::runner<easylocal::runners::FirstImprovement>(
            "fi",
            {.max_evaluations = 100})
        | easylocal::runner<easylocal::runners::FirstImprovement>(
            "fi-swap",
            {.max_evaluations = 100},
            easylocal::neighborhood<SwapCitiesNeighborhoodExplorer>()
                | easylocal::delta<TourLengthComponent, SwapTourLengthDelta>());
}
// [runner-neighborhood] ----------------------------------------------------

} // namespace tsp
