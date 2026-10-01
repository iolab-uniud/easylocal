// The running example of docs/tutorial, chapter by chapter.
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <cstdint>
#include <iostream>
#include <random>
#include <stop_token>
#include <utility>

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;

    const auto tsp = five_cities();

    // [recipes] ------------------------------------------------------------
    auto sm = el::solution_manager<TourManager>()
            | el::component<TourLength>();

    auto nhe = el::neighborhood<TwoOptExplorer>()
             | el::delta<TourLength, TwoOptLengthDelta>();
    // [recipes] ------------------------------------------------------------

    // [first-improvement] --------------------------------------------------
    auto fi = el::make_runner<runners::FirstImprovement>(
                  runners::FirstImprovementParameters{})
              | sm | nhe;

    auto bound = fi.bind(tsp);
    const auto result = bound.run(bound.initial_solution());
    // [first-improvement] --------------------------------------------------

    // [with-spelling] ------------------------------------------------------
    auto same_runner =
        el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            .with_solution_manager(
                el::solution_manager<TourManager>().with_component<TourLength>())
            .with_neighborhood(
                el::neighborhood<TwoOptExplorer>().with_delta<TourLength, TwoOptLengthDelta>());
    // [with-spelling] ------------------------------------------------------

    // [aggregation] --------------------------------------------------------
    auto weighted_sm = el::solution_manager<TourManager>()
                     | el::component<TourLength>()
                     | el::component<MaxEdge>()
                     | el::aggregator(el::cost::weighted_sum{1.0, 10.0});
    // [aggregation] --------------------------------------------------------

    // [annealing] ----------------------------------------------------------
    using Classic = runners::temperature::Classic;

    auto sa = el::make_runner<runners::SimulatedAnnealing<Classic>>(
                  Classic{runners::temperature::ClassicParameters{
                      .initial_temperature = 10.0,
                      .final_temperature = 0.1,
                      .cooling_rate = 0.95,
                      .samples_per_temperature = 50}})
              | sm | nhe;

    std::mt19937_64 rng{42};
    auto sa_bound = sa.bind(tsp);
    const auto annealed = sa_bound.run(sa_bound.initial_solution(), rng);
    // [annealing] ----------------------------------------------------------

    // [union] --------------------------------------------------------------
    auto both = el::neighborhood_union(
                    el::neighborhood<TwoOptExplorer>()
                        | el::delta<TourLength, TwoOptLengthDelta>(),
                    el::neighborhood<SwapExplorer>())
              | el::random_biases(3.0, 1.0);

    auto union_sa = el::make_runner<runners::SimulatedAnnealing<Classic>>(
                        Classic{runners::temperature::ClassicParameters{
                            .initial_temperature = 10.0,
                            .final_temperature = 0.1,
                            .cooling_rate = 0.95,
                            .samples_per_temperature = 50}})
                    | sm | both;
    // [union] --------------------------------------------------------------

    // [custom-runner-use] --------------------------------------------------
    auto descent = el::make_runner<RandomDescent>(RandomDescentParameters{.max_evaluations = 200})
                 | sm | nhe;
    // [custom-runner-use] --------------------------------------------------

    // [solvers] ------------------------------------------------------------
    auto solver = el::make_solver<el::solvers::MultiStart>(
        fi,
        el::solvers::MultiStartConfig<el::initialization::Random>{
            .parameters = {.starts = 5},
            .initialization = el::initialization::random,
            .seed = 1,
        });
    const auto best = solver.solve(tsp);
    // [solvers] ------------------------------------------------------------

    // [configuration] ------------------------------------------------------
    const auto configuration = el::config::root(sa.configuration<"solver">());

    const auto configured = el::config::load_and_apply(argc, argv, configuration);
    if (configured.help_requested)
    {
        std::cout << el::config::cli_help(argv[0], configuration);
        return 0;
    }
    if (!configured)
    {
        el::config::print_diagnostics(std::cerr, configured);
        return 2;
    }
    // [configuration] ------------------------------------------------------

    // [app] ----------------------------------------------------------------
    auto application = el::app("tsp")
        .with_solution_manager(sm)
        .with_neighborhood(nhe)
        .with_runner<runners::FirstImprovement>("fi")
        .with_runner<runners::BestImprovement>("bi", {.max_evaluations = 500});

    auto piped_application = el::app("tsp") | sm | nhe
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::BestImprovement>("bi", {.max_evaluations = 500});

    const auto app_result =
        application.run<runners::FirstImprovement>(tsp, Tour{{0, 1, 2, 3, 4}});
    // [app] ----------------------------------------------------------------

    // [tester] -------------------------------------------------------------
    el::Tester tester{application};
    tester.set_input(tsp);
    tester.use_initial_solution();
    (void)tester.use_first_improving_move();
    (void)tester.run_runner("fi");
    // [tester] -------------------------------------------------------------

    // [control] ------------------------------------------------------------
    std::stop_source stop;
    auto observer = [](const el::run_progress& progress) {
        (void)progress.evaluations; // also: iterations, evaluation_limit
    };
    el::run_control control{stop.get_token(), observer};
    el::trace::memory_recorder<double> trace;

    auto descent_bound = descent.bind(tsp);
    const auto observed = descent_bound.run(
        descent_bound.initial_solution(), rng, el::with(control, trace));
    // [control] ------------------------------------------------------------

    auto same_bound = same_runner.bind(tsp);
    auto weighted = (el::make_runner<runners::FirstImprovement>(
                         runners::FirstImprovementParameters{})
                     | weighted_sm | el::neighborhood<TwoOptExplorer>())
                        .bind(tsp);
    auto union_bound = union_sa.bind(tsp);
    std::mt19937_64 union_rng{7};

    std::cout << "first improvement " << result.cost
              << "\nwith spelling " << same_bound.run(same_bound.initial_solution()).cost
              << "\nweighted " << weighted.run(weighted.initial_solution()).cost
              << "\nannealing " << annealed.cost
              << "\nunion " << union_bound.run(union_bound.initial_solution(), union_rng).cost
              << "\nmulti-start " << best.cost
              << "\napp " << app_result.cost
              << "\ntester " << tester.evaluate()
              << "\ndescent " << observed.cost << " with " << trace.records().size()
              << " trace events\n";
    (void)piped_application;
    return 0;
}
