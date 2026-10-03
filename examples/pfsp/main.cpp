// The permutation flow shop as a command-line program: the tabu search of
// the tabu list study, run by cli::run (--instance, --runners.tabu.*, ...).
#include "makespan_component.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/cli.hpp>
#include <easylocal/runners/tabu_search.hpp>

#ifndef EASYLOCAL_PFSP_INSTANCE_FILE
#error "EASYLOCAL_PFSP_INSTANCE_FILE must name the example instance"
#endif

int main(int argc, char* argv[])
{
    using namespace pfsp;
    using easylocal::runners::TabuSearch;

    // A fixed-length tabu list, aspiration by objective, exhaustive
    // exploration and a stop after idle iterations.
    auto application = easylocal::app("pfsp")
        | (easylocal::solution_manager<PfspSolutionManager>()
            | easylocal::component<MakespanComponent>())
        | easylocal::neighborhood<SwapJobsNeighborhoodExplorer>()
        | easylocal::runner<TabuSearch<>>(
            "tabu",
            {.max_idle_iterations = 1000, .tabu_list = {.tenure = 10}});

    // The search starts from a random schedule, as in the study.
    return easylocal::cli::run(
        application,
        argc,
        argv,
        {.defaults = {
             .instance = EASYLOCAL_PFSP_INSTANCE_FILE,
             .seed = 2026,
             .start = "random",
         }});
}
