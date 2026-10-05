// The tutorial's TSP solved by a pipeline of three stages, each with its own
// runner, neighborhood or cost: a descent on the hard cost until the tour is
// feasible, repeated from new random tours; a descent on the whole cost; a
// hill climbing on random moves of two neighborhoods; then the same stages
// in an app, on its recipes.
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <iostream>
#include <random>

#ifndef EASYLOCAL_TUTORIAL_INSTANCE
#define EASYLOCAL_TUTORIAL_INSTANCE "five.tsp"
#endif

int main()
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;

    const auto tsp = el::load_input<Tsp>(EASYLOCAL_TUTORIAL_INSTANCE);

    // Edges longer than 8 are violations (hard), the length is the objective.
    auto sm = el::solution_manager<TourManager>()
        | el::cost::hard_soft(
            el::cost::apply(
                [](double longest) { return std::max(0.0, longest - 8.0); },
                el::component<MaxEdge>()),
            el::component<TourLength>());
    auto two_opt =
        el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>();
    auto both = el::neighborhood_union(two_opt, el::neighborhood<SwapExplorer>());

    auto descent =
        el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        | sm | two_opt;
    auto climbing =
        el::make_runner<runners::HillClimbing>(
            runners::HillClimbingParameters{.max_idle_iterations = 200})
        | sm | both;

    // [pipeline] -----------------------------------------------------------
    // | chains the stages, & gives a stage its options, in parentheses: the
    // first one works on the hard cost until it is zero, in up to five
    // descents from new random tours; the others continue from its tour on the
    // whole cost.
    using namespace el::solvers;
    auto solver = (stage("feasible", descent) & until_feasible() & attempts(5))
        | stage("descent", descent) | stage("climb", climbing);
    const auto result = solver.seed(7).solve(tsp);
    // [pipeline] -----------------------------------------------------------

    // The result is the last stage's, with the effort of every stage, and
    // reports what each stage did.
    for (const auto& report : result.stages)
    {
        std::cout << "stage " << report.name << ": " << report.attempts << " attempt(s), "
                  << report.evaluations << " evaluations, cost " << report.cost << '\n';
    }
    std::cout << "violations " << result.cost.hard() << ", length " << result.cost.soft()
              << ", evaluations " << result.evaluations << '\n';

    // [app-pipeline] -------------------------------------------------------
    // The same stages registered in an app, as algorithms on the app's
    // recipes: its cost, so its cost.* parameters apply to every stage, and
    // its neighborhood, or the stage's own. The app runs the pipeline from the
    // current solution; restart() starts the first stage's other attempts from
    // new random tours.
    auto application = el::app("tsp") | sm | two_opt
        | el::pipeline(
            "cascade",
            stage<runners::FirstImprovement>("feasible") & until_feasible() & attempts(5)
                & restart(el::initialization::random),
            stage<runners::FirstImprovement>("descent"),
            stage<runners::HillClimbing>("climb", {.max_idle_iterations = 200}, both));
    const auto initial = application.bind(tsp).solution_manager().initial_solution();
    std::mt19937_64 rng{7};
    // run() returns no result for a name that is not registered.
    if (const auto cascade = application.run("cascade", tsp, initial, rng))
    {
        std::cout << "cascade: violations " << cascade->cost.hard() << ", length "
                  << cascade->cost.soft() << '\n';
    }
    // [app-pipeline] -------------------------------------------------------
    return 0;
}
