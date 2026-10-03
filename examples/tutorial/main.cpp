// The running example of docs/tutorial, chapter by chapter.
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <iostream>
#include <random>
#include <stop_token>

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;

    const auto tsp = five_cities();

    // [recipes] ------------------------------------------------------------
    auto sm = el::solution_manager<TourManager>() | el::component<TourLength>();

    auto nhe =
        el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>();
    // [recipes] ------------------------------------------------------------

    // [first-improvement] --------------------------------------------------
    auto fi =
        el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        | sm | nhe;

    auto search = fi.bind(tsp);
    const auto result = search.run(search.initial_solution());
    // [first-improvement] --------------------------------------------------

    // [with-spelling] ------------------------------------------------------
    auto same_runner =
        el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            .with_solution_manager(
                el::solution_manager<TourManager>().with_cost(
                    el::component<TourLength>()))
            .with_neighborhood(
                el::neighborhood<TwoOptExplorer>()
                    .with_delta<TourLength, TwoOptLengthDelta>());
    // [with-spelling] ------------------------------------------------------

    // [cost-expression] --------------------------------------------------------
    auto weighted_sm = el::solution_manager<TourManager>()
        | el::cost::sum(el::component<TourLength>(), el::component<MaxEdge>() * 10.0);
    // [cost-expression] --------------------------------------------------------

    // [annealing] ----------------------------------------------------------
    using Classic = runners::temperature::Classic;

    auto sa =
        el::make_runner<runners::SimulatedAnnealing<Classic>>(
            Classic{runners::temperature::ClassicParameters{
                .initial_temperature = 10.0,
                .final_temperature = 0.1,
                .cooling_rate = 0.95,
                .samples_per_temperature = 50}})
        | sm | nhe;

    std::mt19937_64 rng{42};
    auto sa_search = sa.bind(tsp);
    const auto annealed = sa_search.run(sa_search.initial_solution(), rng);
    // [annealing] ----------------------------------------------------------

    // [union] --------------------------------------------------------------
    auto both =
        el::neighborhood_union(
            el::neighborhood<TwoOptExplorer>()
                | el::delta<TourLength, TwoOptLengthDelta>(),
            el::neighborhood<SwapExplorer>())
        | el::random_biases(3.0, 1.0);

    auto union_sa =
        el::make_runner<runners::SimulatedAnnealing<Classic>>(
            Classic{runners::temperature::ClassicParameters{
                .initial_temperature = 10.0,
                .final_temperature = 0.1,
                .cooling_rate = 0.95,
                .samples_per_temperature = 50}})
        | sm | both;
    // [union] --------------------------------------------------------------

    // [custom-runner-use] --------------------------------------------------
    auto descent =
        el::make_runner<RandomDescent>(RandomDescentParameters{.max_evaluations = 200})
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
    auto application =
        el::app("tsp")
            .with_solution_manager(sm)
            .with_neighborhood(nhe)
            .with_runner<runners::FirstImprovement>("fi")
            .with_runner<runners::SimulatedAnnealing<Classic>>(
                "sa",
                {.samples_per_temperature = 50});

    auto piped_application = el::app("tsp") | sm | nhe
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::SimulatedAnnealing<Classic>>(
            "sa",
            {.samples_per_temperature = 50});

    const auto app_result =
        application.run<runners::FirstImprovement>(tsp, Tour{{0, 1, 2, 3, 4}});
    // [app] ----------------------------------------------------------------

    // [check] --------------------------------------------------------------
    const auto report = el::check(application, tsp); // also: check(app, input, solution)
    el::print_report(std::cout, report);
    if (!report)
        return 1;
    // [check] --------------------------------------------------------------

    // [tester] -------------------------------------------------------------
    el::Tester tester{application, /* seed */ 2026};
    tester.set_input(tsp);
    tester.use_initial_solution();
    (void)tester.use_first_improving_move();
    (void)tester.run_runner("sa"); // receives the Tester's RNG
    (void)tester.run_runner("fi");
    // [tester] -------------------------------------------------------------

    // [tester-checks] ------------------------------------------------------
    const auto costs = tester.check_neighborhood_costs(); // delta vs full evaluation
    const auto independence = tester.check_move_independence(); // null and repeated moves
    const auto sampling = tester.check_random_move_distribution(tester.rng());
    if (costs.mismatches != 0 || costs.invalid != 0 || sampling.out_of_neighborhood != 0)
        return 1;
    // [tester-checks] ------------------------------------------------------

    // [control] ------------------------------------------------------------
    std::stop_source stop;
    auto observer = [](const el::run_progress& progress) {
        (void)progress.evaluations; // also: iterations, evaluation_limit
    };
    el::run_control control{stop.get_token(), observer};
    el::trace::memory_recorder<double> trace;

    auto descent_search = descent.bind(tsp);
    const auto observed = descent_search.run(
        descent_search.initial_solution(),
        rng,
        el::with(control, trace));
    // [control] ------------------------------------------------------------

    auto same_search = same_runner.bind(tsp);
    auto weighted =
        (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            | weighted_sm | el::neighborhood<TwoOptExplorer>())
            .bind(tsp);
    auto union_search = union_sa.bind(tsp);
    std::mt19937_64 union_rng{7};

    std::cout
        << "first improvement " << result.cost << "\nwith spelling "
        << same_search.run(same_search.initial_solution()).cost << "\nweighted "
        << weighted.run(weighted.initial_solution()).cost << "\nannealing "
        << annealed.cost << "\nunion "
        << union_search.run(union_search.initial_solution(), union_rng).cost
        << "\nmulti-start " << best.cost << "\napp " << app_result.cost << "\ntester "
        << tester.evaluate() << " (" << costs.moves << " moves checked, "
        << independence.null_moves << " null moves, " << sampling.unseen
        << " moves never sampled)"
        << "\ndescent " << observed.cost << " with " << trace.records().size()
        << " trace events\n";
    (void)piped_application;
    return 0;
}
