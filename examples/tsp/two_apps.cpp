#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <iostream>

#ifndef EASYLOCAL_TSP_MWE_INSTANCE_FILE
#error "EASYLOCAL_TSP_MWE_INSTANCE_FILE must name the example instance"
#endif

int main()
{
    using namespace easylocal::mwe::tsp;

    auto two_opt =
        easylocal::app("tsp-two-opt")
            .solution_manager(
                easylocal::solution_manager<TspSolutionManager>()
                | easylocal::component<TourLengthComponent>()
                | easylocal::aggregator(TourLengthCost{}))
            .neighborhood(
                easylocal::neighborhood<TwoOptNeighborhoodExplorer>()
                | easylocal::delta<
                      TourLengthComponent,
                      TwoOptTourLengthDeltaEvaluator>())
            .runner<easylocal::runners::FirstImprovement>("fi");

    auto swap =
        easylocal::app("tsp-swap")
            .solution_manager(
                easylocal::solution_manager<TspSolutionManager>()
                | easylocal::component<TourLengthComponent>()
                | easylocal::aggregator(TourLengthCost{}))
            .neighborhood(
                easylocal::neighborhood<SwapCitiesNeighborhoodExplorer>()
                | easylocal::delta<
                      TourLengthComponent,
                      SwapTourLengthDeltaEvaluator>())
            .runner<easylocal::runners::FirstImprovement>("fi");

    two_opt.runner_config<easylocal::runners::FirstImprovement>().max_evaluations = 100;
    swap.runner_config<easylocal::runners::FirstImprovement>().max_evaluations = 100;

    const auto instance = load_instance(EASYLOCAL_TSP_MWE_INSTANCE_FILE);
    const Tour initial{
        .tour = {0, 2, 4, 1, 5, 3},
    };

    auto two_opt_runtime = two_opt.for_input(instance);
    const auto first =
        two_opt_runtime.run<easylocal::runners::FirstImprovement>(initial);

    auto swap_runtime = swap.for_input(instance);
    const auto second =
        swap_runtime.run<easylocal::runners::FirstImprovement>(first.solution);

    std::cout << "two-opt cost: " << first.cost << '\n';
    std::cout << "swap cost:    " << second.cost << '\n';

    return 0;
}
