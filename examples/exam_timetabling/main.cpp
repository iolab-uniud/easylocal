// The exam timetabling problem as a command-line program: an app with
// Simulated Annealing, run by cli::run (--instance, --runners.sa.*, ...).
#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/cli.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#ifndef EASYLOCAL_EXAM_INSTANCE_FILE
#error "EASYLOCAL_EXAM_INSTANCE_FILE must name the example instance"
#endif

int main(int argc, char* argv[])
{
    using namespace exam_timetabling;
    using easylocal::component;
    using easylocal::runners::SimulatedAnnealing;
    using easylocal::runners::temperature::FixedLength;

    // This example spells the recipes with with_* calls; the others use pipes.
    auto sm = easylocal::solution_manager<ExamTimetablingSolutionManager>().with_cost(
        easylocal::cost::sum(
            component<StudentConflictComponent>() * 1000,
            component<ConsecutiveExamComponent>() * 10,
            component<TimeslotLoadComponent>()));

    auto nhe =
        easylocal::neighborhood<MoveExamNeighborhoodExplorer>()
            .with_delta<StudentConflictComponent>()
            .with_delta<ConsecutiveExamComponent, ConsecutiveExamDeltaEvaluator>();
    // TimeslotLoadComponent has no delta (see cost_deltas.hpp): EasyLocal
    // re-evaluates it on a candidate solution.

    auto application =
        easylocal::app("exam-timetabling")
            .with_solution_manager(sm)
            .with_neighborhood(nhe)
            .with_runner<SimulatedAnnealing<FixedLength>>(
                "sa",
                {.temperature = {
                     .initial_temperature = 100.0,
                     .final_temperature = 1.0,
                     .cooling_rate = 0.5,
                     .allowed_iterations = 30,
                 }});

    return easylocal::cli::run(
        application,
        argc,
        argv,
        {.defaults = {
             .instance = EASYLOCAL_EXAM_INSTANCE_FILE,
             .seed = 2026,
             .start = "initial",
         }});
}
