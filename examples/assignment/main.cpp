#include "instance_io.hpp" // IWYU pragma: keep (the read_input hook, found by ADL)
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/app/io.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/config/cli.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers.hpp>

#include <filesystem>
#include <iostream>
#include <utility>

#ifndef EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE
#error "EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE must name the example instance"
#endif

namespace
{

using namespace assignment;

struct AppParameters
{
    std::filesystem::path instance_file;

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"instance_file", &AppParameters::instance_file>(
                "Assignment instance file"));
    }

    easylocal::config::validation_result validate() const
    {
        if (instance_file.empty())
        {
            return easylocal::config::validation_result::failure(
                "instance_file must not be empty");
        }

        return easylocal::config::validation_result::success();
    }
};

} // namespace

int main(int argc, char* argv[])
{
    using namespace assignment;
    using easylocal::make_runner;
    using easylocal::make_solver;
    using easylocal::neighborhood;
    using easylocal::solution_manager;
    using easylocal::runners::FirstImprovementParameters;

    try
    {
        AppParameters app_parameters{
            .instance_file = EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE,
        };
        FirstImprovementParameters search_parameters{
            .max_evaluations = 100,
        };

        // The cost is hierarchical (see cost.hpp): the capacity violation
        // has strict priority over the load imbalance. No delta is bound, so
        // moves are evaluated on a candidate solution.
        auto runner = make_runner<easylocal::runners::FirstImprovement>(search_parameters)
            | (solution_manager<AssignmentSolutionManager>() | assignment_cost())
            | neighborhood<ReassignJobNeighborhoodExplorer>();

        // The program's parameters under "application", the runner's under
        // "solver", the run's under "run": --application.instance_file,
        // --solver.search.*, --run.target.
        easylocal::RunParameters run_parameters;
        easylocal::config::parameter_set configuration;
        configuration.add("application", app_parameters);
        configuration.add("solver", runner.configuration());
        configuration.add("run", run_parameters);

        const auto configured =
            easylocal::config::load_and_apply(argc, argv, configuration);
        if (configured.help_requested)
        {
            std::cout << easylocal::config::cli_help(argv[0], configuration);
            return 0;
        }

        if (!configured)
        {
            easylocal::config::print_diagnostics(std::cerr, configured);
            return 2;
        }

        const auto instance =
            easylocal::load_input<AssignmentInstance>(app_parameters.instance_file);
        const auto bound = runner.bind(instance);
        const auto initial_solution = bound.initial_solution();
        using cost_type = decltype(bound)::cost_type;
        const auto target = run_parameters.target_cost<cost_type>(instance);

        auto solver = make_solver<easylocal::solvers::TwoStage>(
            std::move(runner),
            easylocal::solvers::TwoStageConfig<easylocal::initialization::Initial>{
                .initialization = easylocal::initialization::initial,
                .seed = 0,
            });
        // With a target, the second stage stops at the first solution that
        // reaches it, such as [[0, 0], 0].
        const auto result = target
            ? solver.solve(instance, easylocal::stop_at(*target))
            : solver.solve(instance);

        std::cout << "instance:         " << app_parameters.instance_file << '\n';
        std::cout << "initial solution: " << easylocal::describe(initial_solution)
                  << '\n';
        std::cout << "final solution:   " << easylocal::describe(result.solution) << '\n';

        std::cout << "final hard cost: overload=" << result.cost.hard().get<0>()
                  << ", overloaded_machines=" << result.cost.hard().get<1>() << '\n';
        std::cout << "final soft cost: load_imbalance=" << result.cost.soft() << '\n';
        std::cout << "evaluations: " << result.evaluations << '\n';
        std::cout << "termination: " << easylocal::to_string(result.termination) << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
