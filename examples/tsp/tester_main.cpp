#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/adapters/tui/launcher.hpp>

#include <utility>

#ifndef EASYLOCAL_TSP_MWE_INSTANCE_FILE
#error "EASYLOCAL_TSP_MWE_INSTANCE_FILE must name the example instance"
#endif

#ifndef EASYLOCAL_TSP_MWE_SOLUTION_FILE
#error "EASYLOCAL_TSP_MWE_SOLUTION_FILE must name the example solution"
#endif

int main()
{
    using namespace easylocal::mwe::tsp;

    auto two_opt =
        easylocal::app("tsp-two-opt")
            .with_solution_manager(
                easylocal::solution_manager<TspSolutionManager>()
                | easylocal::component<TourLengthComponent>()
                | easylocal::aggregator(TourLengthCost{}))
            .with_neighborhood(
                easylocal::neighborhood<TwoOptNeighborhoodExplorer>()
                | easylocal::delta<
                      TourLengthComponent,
                      TwoOptTourLengthDeltaEvaluator>())
            .with_runner<easylocal::runners::FirstImprovement>("fi");

    auto swap =
        easylocal::app("tsp-swap")
            .with_solution_manager(
                easylocal::solution_manager<TspSolutionManager>()
                | easylocal::component<TourLengthComponent>()
                | easylocal::aggregator(TourLengthCost{}))
            .with_neighborhood(
                easylocal::neighborhood<SwapCitiesNeighborhoodExplorer>()
                | easylocal::delta<
                      TourLengthComponent,
                      SwapTourLengthDeltaEvaluator>())
            .with_runner<easylocal::runners::FirstImprovement>("fi");

    two_opt.runner_config<easylocal::runners::FirstImprovement>().max_evaluations = 100;
    swap.runner_config<easylocal::runners::FirstImprovement>().max_evaluations = 100;

    easylocal::tui::run_launcher(
        {
            .title = "EasyLocal++ TSP Tester",
            .tester = {
                .seed = 0,
                .input_path = EASYLOCAL_TSP_MWE_INSTANCE_FILE,
                .solution_path = EASYLOCAL_TSP_MWE_SOLUTION_FILE,
            },
        },
        std::move(two_opt),
        std::move(swap));
}
