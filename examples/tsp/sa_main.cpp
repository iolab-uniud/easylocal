// The TSP as a command-line program: Simulated Annealing on the union of the
// 2-opt and swap neighborhoods, run by cli::run (--instance, --runners.sa.*,
// --neighborhood.random_biases, ...).
#include "apps.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/cli.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#ifndef EASYLOCAL_TSP_INSTANCE_FILE
#error "EASYLOCAL_TSP_INSTANCE_FILE must name the example instance"
#endif

int main(int argc, char* argv[])
{
    using namespace tsp;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::runners::SimulatedAnnealing;
    using easylocal::runners::temperature::FixedLength;

    // TourLengthValue is a domain value: tsp_solution_manager() maps it to the
    // scalar cost with TourLengthCost (apps.hpp).
    auto application = easylocal::app("tsp-sa") | tsp_solution_manager()
        | (easylocal::neighborhood_union(
               neighborhood<TwoOptNeighborhoodExplorer>()
                   | delta<TourLengthComponent, TwoOptTourLengthDeltaEvaluator>(),
               neighborhood<SwapCitiesNeighborhoodExplorer>()
                   | delta<TourLengthComponent, SwapTourLengthDeltaEvaluator>())
            | easylocal::random_biases(3.0, 1.0))
        | easylocal::runner<SimulatedAnnealing<FixedLength>>(
            "sa",
            {.temperature = {
                 .initial_temperature = 8.0,
                 .final_temperature = 0.25,
                 .cooling_rate = 0.75,
                 .max_iterations = 200,
             }});

    return easylocal::cli::run(
        application,
        argc,
        argv,
        {.defaults = {
             .instance = EASYLOCAL_TSP_INSTANCE_FILE,
             .seed = 2026,
             .start = "initial",
         }});
}
