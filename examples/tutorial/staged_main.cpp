// The tutorial's TSP behind a main shaped like a real EasyLocal 3 solver
// (docs/from-easylocal-3.md): a hierarchical cost, parameters adjusted to the instance, a
// two-stage solve and a report per component.
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>

#ifndef EASYLOCAL_TUTORIAL_INSTANCE
#define EASYLOCAL_TUTORIAL_INSTANCE "five.tsp"
#endif

namespace
{

struct MainParameters
{
    std::filesystem::path instance;
    std::uint64_t seed{0};
    std::size_t evaluations_per_city{0};

    static consteval auto parameter_schema()
    {
        namespace config = easylocal::config;
        return config::fields(
            config::field<"instance", &MainParameters::instance>(
                "Input instance",
                easylocal::unlimited),
            config::field<"seed", &MainParameters::seed>(
                "Random seed",
                easylocal::unlimited),
            config::field<"evaluations_per_city", &MainParameters::evaluations_per_city>(
                "Descent budget per city (0: until a local optimum)",
                config::range(0, easylocal::unlimited)));
    }

    easylocal::config::validation_result validate() const
    {
        if (instance.empty())
            return easylocal::config::validation_result::failure("instance must be set");
        return easylocal::config::validation_result::success();
    }
};

} // namespace

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;

    // [staged-recipes] -----------------------------------------------------
    // One hierarchical cost: edges longer than 8 are violations (hard), the
    // length is the objective (soft). EasyLocal 3 needed a SolutionManager
    // for each set of components (all, hard only); with_hard_cost() derives
    // the hard-only runner from this one.
    auto sm = el::solution_manager<TourManager>()
        | el::cost::hard_soft(
            el::cost::apply(
                [](double longest) { return std::max(0.0, longest - 8.0); },
                el::component<MaxEdge>()),
            el::component<TourLength>());

    // The explorer and its deltas are written once: the hard-only stage
    // ignores the delta of the soft TourLength.
    auto descent =
        el::make_runner<runners::FirstImprovement>(runners::FirstImprovementParameters{})
        | sm
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>());
    // [staged-recipes] -----------------------------------------------------

    // [staged-command-line] ------------------------------------------------
    // The runner's parameters under its own prefix: --descent.search.*, and
    // --descent.cost.* for the weights of a cost that has them.
    MainParameters main_parameters{.instance = EASYLOCAL_TUTORIAL_INSTANCE};
    el::config::parameter_set configuration;
    configuration.add("main", main_parameters);
    configuration.add("descent", descent.configuration());

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
    // [staged-command-line] ------------------------------------------------

    // [staged-instance] ----------------------------------------------------
    // Parameters that depend on the instance, set once it is read, as
    // EasyLocal 3's SetParameter("max_evaluations", ...) after the parsing.
    const auto tsp = el::load_input<Tsp>(main_parameters.instance);
    if (main_parameters.evaluations_per_city != 0)
        descent.parameters().max_evaluations =
            main_parameters.evaluations_per_city * tsp.cities();
    // [staged-instance] ----------------------------------------------------

    // [staged-run] ---------------------------------------------------------
    // EasyLocal 3's two solvers, one per SolutionManager, and the Resolve that
    // passed the solution from one to the other: the descent on the hard cost
    // from a random tour until it is feasible, then on the whole cost.
    auto stages =
        el::solvers::two_stage(descent)
            .initialization(el::initialization::random)
            .seed(main_parameters.seed);
    const auto result = stages.solve(tsp);
    // [staged-run] ---------------------------------------------------------

    // [staged-report] ------------------------------------------------------
    std::cout << "violations " << result.cost.hard() << ", length " << result.cost.soft()
              << '\n'
              << "iterations " << result.iterations // of both stages
              << ", termination " << el::to_string(result.termination) << '\n';
    // The value of each component: components are plain classes, constructed
    // from the Input (EasyLocal 3's cc.Cost(out) on each component).
    std::cout << "TourLength " << TourLength{tsp}.evaluate(result.solution)
              << ", MaxEdge " << MaxEdge{tsp}.evaluate(result.solution) << '\n';
    el::write_solution(tsp, result.solution, std::cout);
    // [staged-report] ------------------------------------------------------
    return 0;
}
