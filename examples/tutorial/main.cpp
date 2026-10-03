// The running example of docs/tutorial, chapter by chapter.
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <iostream>
#include <random>
#include <stop_token>

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;

    const auto tsp = five_cities();

    // [sm-recipe] ----------------------------------------------------------
    auto sm = el::solution_manager<TourManager>() | el::component<TourLength>();
    // [sm-recipe] ----------------------------------------------------------

    // [nhe-recipe] ---------------------------------------------------------
    auto nhe =
        el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>();
    // [nhe-recipe] ---------------------------------------------------------

    // [co-located-recipe] --------------------------------------------------
    auto colocated_sm =
        el::solution_manager<TourManager>() | el::component<TourLengthWithDelta>();
    auto colocated_nhe =
        el::neighborhood<TwoOptExplorer>() | el::delta<TourLengthWithDelta>();
    // [co-located-recipe] --------------------------------------------------

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

    // [cost-expression] ----------------------------------------------------
    auto weighted_sm = el::solution_manager<TourManager>()
        | el::cost::sum(el::component<TourLength>(), el::component<MaxEdge>() * 10.0);
    // [cost-expression] ----------------------------------------------------

    // [domain-value-recipe] ------------------------------------------------
    // The struct is the cost: it is compared with its operator<=>.
    auto long_edges_sm =
        el::solution_manager<TourManager>() | el::component<LongEdgesComponent>();

    // A function turns it into a number: here the excess is a hard cost.
    auto excess_sm = el::solution_manager<TourManager>()
        | el::cost::hard_soft(
            el::cost::apply(
                [](const LongEdges& value) { return value.excess; },
                el::component<LongEdgesComponent>()),
            el::component<TourLength>());
    // [domain-value-recipe] ------------------------------------------------

    // [structured-costs] ---------------------------------------------------
    // Lexicographic: the longest edge first; between tours with the same
    // longest edge, the shorter one.
    auto bottleneck_sm = el::solution_manager<TourManager>()
        | el::cost::in_order(el::component<MaxEdge>(), el::component<TourLength>());

    // Hierarchical: edges longer than 8 are a violation, measured by how much
    // the longest edge exceeds 8 (hard); the tour length is the objective (soft).
    auto bounded_sm = el::solution_manager<TourManager>()
        | el::cost::hard_soft(
            el::cost::apply(
                [](double longest) { return std::max(0.0, longest - 8.0); },
                el::component<MaxEdge>()),
            el::component<TourLength>());
    // [structured-costs] ---------------------------------------------------

    // [structured-costs-read] ----------------------------------------------
    auto bottleneck =
        (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            | bottleneck_sm | nhe)
            .bind(tsp);
    const auto by_edge = bottleneck.run(bottleneck.initial_solution());
    // A lexicographic cost is read by position, a hierarchical one by branch.
    const double longest_edge = by_edge.cost.get<0>();
    const double length_after_edge = by_edge.cost.get<1>();

    auto bounded =
        (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            | bounded_sm | nhe)
            .bind(tsp);
    const auto within_bound = bounded.run(bounded.initial_solution());
    const double excess = within_bound.cost.hard();
    const double length_within_bound = within_bound.cost.soft();
    // [structured-costs-read] ----------------------------------------------

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

    // [app-in-main] --------------------------------------------------------
    // A runtime holds the services of the app for one Input.
    auto runtime = application.for_input(tsp);
    const auto initial = runtime.solution_manager().initial_solution();

    // Run the registered runners by algorithm, each from the same tour.
    const auto by_descent = runtime.run<runners::FirstImprovement>(initial);
    std::mt19937_64 annealing_rng{2026};
    const auto by_annealing =
        runtime.run<runners::SimulatedAnnealing<Classic>>(initial, annealing_rng);
    // [app-in-main] --------------------------------------------------------

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

    // Select the first improving move, if there is one, and apply it.
    if (tester.use_first_improving_move())
        tester.apply_move();

    // Run registered runners by name; false means no runner has that name.
    if (!tester.run_runner("sa")) // receives the Tester's RNG
        return 1;
    if (!tester.run_runner("fi"))
        return 1;
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
    auto long_edges =
        (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            | long_edges_sm | el::neighborhood<TwoOptExplorer>())
            .bind(tsp);
    const LongEdges fewest = long_edges.run(long_edges.initial_solution()).cost;
    auto excess_search =
        (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            | excess_sm | nhe)
            .bind(tsp);
    const auto excess_cost = excess_search.run(excess_search.initial_solution()).cost;

    auto colocated =
        (el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
            | colocated_sm | colocated_nhe)
            .bind(tsp);
    auto union_search = union_sa.bind(tsp);
    std::mt19937_64 union_rng{7};

    std::cout
        << "first improvement " << result.cost << "\nwith spelling "
        << same_search.run(same_search.initial_solution()).cost << "\nweighted "
        << weighted.run(weighted.initial_solution()).cost << "\nlexicographic "
        << longest_edge << ", " << length_after_edge << "\nhierarchical " << excess
        << ", " << length_within_bound << "\nlong edges " << fewest.excess << " in "
        << fewest.count << "\nexcess " << excess_cost.hard() << ", " << excess_cost.soft()
        << "\nco-located " << colocated.run(colocated.initial_solution()).cost
        << "\nannealing " << annealed.cost << "\nunion "
        << union_search.run(union_search.initial_solution(), union_rng).cost
        << "\nmulti-start " << best.cost << "\napp " << app_result.cost << " ("
        << by_descent.cost << ", " << by_annealing.cost << ")" << "\ntester "
        << tester.evaluate() << " (" << costs.moves << " moves checked, "
        << independence.null_moves << " null moves, " << sampling.unseen
        << " moves never sampled)"
        << "\ndescent " << observed.cost << " with " << trace.records().size()
        << " trace events\n";
    (void)piped_application;
    return 0;
}
