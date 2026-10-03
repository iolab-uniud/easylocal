// The tutorial's TSP as a command-line program (chapter 11): the app, run by
// cli::run, which reads the instance, the seed and the runner from the
// command line.
#include "tsp.hpp"

#include <easylocal/app/cli.hpp>
#include <easylocal/easylocal.hpp>

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;
    using Classic = runners::temperature::Classic;

    // [cli] ----------------------------------------------------------------
    auto application = el::app("tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::SimulatedAnnealing<Classic>>(
            "sa",
            {.temperature = {.samples_per_temperature = 50}});

    return el::cli::run(application, argc, argv);
    // [cli] ----------------------------------------------------------------
}
