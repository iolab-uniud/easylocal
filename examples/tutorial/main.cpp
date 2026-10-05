// The running example of docs/tutorial, chapter by chapter.
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stop_token>

int main(int argc, char* argv[])
{
    // [aliases] ------------------------------------------------------------
    using namespace tutorial;               // the tutorial's types: Tsp, Tour, ...
    namespace el = easylocal;               // el::app, el::component, ...
    namespace runners = easylocal::runners; // runners::FirstImprovement, ...
    // [aliases] ------------------------------------------------------------

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

    // [load-and-print] -----------------------------------------------------
    // The same five cities, read from a file with the read_input hook.
    const auto from_file = el::load_input<Tsp>(EASYLOCAL_TUTORIAL_INSTANCE);
    auto file_search = fi.bind(from_file);
    const auto file_result = file_search.run(file_search.initial_solution());
    std::cout << "from file " << el::describe(file_result.solution) << '\n';
    // [load-and-print] -----------------------------------------------------

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

    // [cost-parameters-use] ------------------------------------------------
    // The same hierarchical cost, its bound a parameter: cost.excess.bound.
    auto limited =
        el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        | (el::solution_manager<TourManager>()
            | el::cost::hard_soft(
                el::cost::apply<Excess>({.bound = 8.0}, el::component<MaxEdge>()),
                el::component<TourLength>()))
        | nhe;
    // [cost-parameters-use] ------------------------------------------------

    // [annealing] ----------------------------------------------------------
    using Classic = runners::temperature::Classic;

    auto sa =
        el::make_runner<runners::SimulatedAnnealing<Classic>>({
            .temperature =
                {
                    .initial_temperature = 10.0,
                    .final_temperature = 0.1,
                    .cooling_rate = 0.95,
                    .samples_per_temperature = 50,
                },
        })
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
        el::make_runner<runners::SimulatedAnnealing<Classic>>({
            .temperature =
                {
                    .initial_temperature = 10.0,
                    .final_temperature = 0.1,
                    .cooling_rate = 0.95,
                    .samples_per_temperature = 50,
                },
        })
        | sm | both;
    // [union] --------------------------------------------------------------

    // [custom-runner-use] --------------------------------------------------
    auto descent =
        el::make_runner<RandomDescent>(RandomDescentParameters{.max_evaluations = 200})
        | sm | nhe;
    // [custom-runner-use] --------------------------------------------------

    // [solvers] ------------------------------------------------------------
    el::solvers::MultiStart solver{fi, {.starts = 5}};
    solver.initialization(el::initialization::random).seed(1);
    const auto best = solver.solve(tsp);
    // [solvers] ------------------------------------------------------------

    // [configuration] ------------------------------------------------------
    el::config::parameter_set configuration;
    configuration.add("solver", sa.configuration());       // --solver.search.*
    configuration.add("limited", limited.configuration()); // --limited.cost.*

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
                {.temperature = {.samples_per_temperature = 50}});

    auto piped_application = el::app("tsp") | sm | nhe
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::SimulatedAnnealing<Classic>>(
            "sa",
            {.temperature = {.samples_per_temperature = 50}});
    // [app] ----------------------------------------------------------------

    // [session] ------------------------------------------------------------
    // The app on one Input, which the session owns, with a seeded RNG.
    el::Session session{application, tsp, /* seed */ 2026};
    session.use_initial_solution();

    // A step by hand: select the first improving move, if any, and apply it.
    if (session.use_first_improving_move())
        session.apply_move();

    // A registered runner, by name, from the current solution; false means no
    // runner has that name.
    if (!session.run("sa")) // receives the session's RNG
        return 1;
    const double session_cost = session.evaluate();
    const Tour session_tour = session.solution(); // a copy: the session goes on
    // [session] ------------------------------------------------------------

    // [check] --------------------------------------------------------------
    const auto report = el::check(application, tsp); // also: check(app, input, solution)
    el::print_report(std::cout, report);
    if (!report)
        return 1;
    // [check] --------------------------------------------------------------

    // [session-checks] -----------------------------------------------------
    // Each check enumerates the neighborhood of the current solution and
    // returns a struct of counters (Session::..._result).

    // The delta evaluation of each move against the full evaluation of the
    // solution it leads to: moves (enumerated), mismatches (the two costs
    // differ), invalid (moves that are not valid or lead to an invalid
    // solution).
    const auto costs = session.check_neighborhood_costs();

    // What each move does to the solution: moves, null_moves (moves that leave
    // it unchanged), repeated_states (moves that lead to a solution an earlier
    // move reached), invalid.
    const auto independence = session.check_move_independence();

    // random_move against the enumeration: neighborhood_size (valid enumerated
    // moves), samples (draws, 20 per move by default), out_of_neighborhood
    // (draws that return no move or one the enumeration does not contain),
    // unseen (enumerated moves never drawn), min_frequency and max_frequency
    // (how often the least and the most drawn moves came up).
    const auto sampling = session.check_random_move_distribution(session.rng());

    if (costs.mismatches != 0 || costs.invalid != 0 || sampling.out_of_neighborhood != 0)
        return 1;
    // [session-checks] -----------------------------------------------------

    // [session-parameters] -------------------------------------------------
    // The app's parameters by path; they change this session's app only.
    const std::array changes{
        el::config::text_override{"runners.sa.temperature.cooling_rate", "0.9"}};
    if (!session.configure(changes)) // all or none, checked
        return 1;

    // A run that stops at the first tour of length 26 or less.
    session.use_initial_solution();
    if (!session.run("sa", el::stop_at(session.read_cost("26"))))
        return 1;
    // [session-parameters] -------------------------------------------------

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

    auto limited_search = limited.bind(tsp);
    const auto limited_cost = limited_search.run(limited_search.initial_solution()).cost;

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
        << "\nlimited " << limited_cost.hard() << ", " << limited_cost.soft()
        << "\nco-located " << colocated.run(colocated.initial_solution()).cost
        << "\nannealing " << annealed.cost << "\nunion "
        << union_search.run(union_search.initial_solution(), union_rng).cost
        << "\nmulti-start " << best.cost << "\nsession " << session_cost << " ("
        << describe(session_tour) << "), " << costs.moves << " moves checked, "
        << independence.null_moves << " null moves, " << sampling.unseen
        << " moves never sampled"
        << "\ndescent " << observed.cost << " with " << trace.records().size()
        << " trace events\n";
    (void)piped_application;
    return 0;
}
