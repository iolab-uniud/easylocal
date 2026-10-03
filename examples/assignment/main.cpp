#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/config/cli.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers.hpp>

#include <cstddef>
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

void print_solution(const AssignmentSolution& solution)
{
    std::cout << '[';

    for (std::size_t job = 0; job < solution.assignment.size(); ++job)
    {
        if (job != 0)
            std::cout << ", ";

        std::cout << solution.assignment[job];
    }

    std::cout << ']';
}

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
        // "solver": --application.instance_file, --solver.search.*.
        easylocal::config::parameter_set configuration;
        configuration.add("application", app_parameters);
        configuration.add("solver", runner.configuration());

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

        const auto instance = load_instance(app_parameters.instance_file);
        const auto initial_solution = runner.bind(instance).initial_solution();

        auto solver = make_solver<easylocal::solvers::TwoStage>(
            std::move(runner),
            easylocal::solvers::TwoStageConfig<easylocal::initialization::Initial>{
                .initialization = easylocal::initialization::initial,
                .seed = 0,
            });
        const auto result = solver.solve(instance);

        std::cout << "instance:         " << app_parameters.instance_file << '\n';
        std::cout << "initial solution: ";
        print_solution(initial_solution);
        std::cout << '\n';

        std::cout << "final solution:   ";
        print_solution(result.solution);
        std::cout << '\n';

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
