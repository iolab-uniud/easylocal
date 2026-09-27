#include "capacity_delta.hpp"
#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/config/cli.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/solver.hpp>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <type_traits>

#ifndef EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE
#error "EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE must name the example instance"
#endif

namespace
{

using namespace easylocal::mwe::assignment;

struct AppParameters
{
    std::filesystem::path instance_file;

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "instance_file",
                &AppParameters::instance_file>(
                    "Assignment instance file"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        if (instance_file.empty())
        {
            return easylocal::config::validation_result::failure(
                "instance_file must not be empty");
        }

        return easylocal::config::validation_result::success();
    }
};

template<class Tree>
void print_configuration(const Tree& tree)
{
    std::cout << "configuration:\n";
    easylocal::config::for_each_config_parameter(
        tree,
        [](const auto path, const auto, const auto&) {
            using path_type = std::remove_cvref_t<decltype(path)>;
            bool first = true;
            std::cout << "  ";
            for (const auto segment : path_type::segments())
            {
                if (!first)
                {
                    std::cout << '.';
                }
                std::cout << segment;
                first = false;
            }
            std::cout << '\n';
        });
}

void print_solution(const AssignmentSolution& solution)
{
    std::cout << '[';

    for (std::size_t job = 0; job < solution.assignment.size(); ++job)
    {
        if (job != 0)
        {
            std::cout << ", ";
        }

        std::cout << solution.assignment[job];
    }

    std::cout << ']';
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace easylocal::mwe::assignment;
    using easylocal::aggregator;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::make_runner;
    using easylocal::make_solver;
    using easylocal::neighborhood;
    using easylocal::solution_manager;
    using easylocal::search::FirstImprovementParameters;
    using easylocal::search::FirstImprovementTermination;

    try
    {
        AppParameters app_parameters{
            .instance_file = EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE,
        };
        FirstImprovementParameters search_parameters{
            .max_evaluations = 100,
        };

        // Equivalent fluent spelling for the SolutionManager recipe:
        // auto sm = solution_manager<AssignmentSolutionManager>()
        //     .with_component<CapacityCostComponent>()
        //     .with_component<LoadImbalanceCostComponent>()
        //     .with_aggregator(AssignmentCostAggregator{});
        auto runner =
            make_runner<easylocal::runner::first_improvement>(search_parameters)
            | (solution_manager<AssignmentSolutionManager>()
               | component<CapacityCostComponent>()
               | component<LoadImbalanceCostComponent>()
               | aggregator(AssignmentCostAggregator{}))
            | (neighborhood<ReassignJobNeighborhoodExplorer>()
               | delta<
                     CapacityCostComponent,
                     ReassignCapacityDeltaEvaluator>());

        const auto configuration = easylocal::config::root(
            easylocal::config::named<"application">(app_parameters),
            runner.configuration<"solver">());

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

        print_configuration(configuration);

        const auto instance = load_instance(app_parameters.instance_file);
        const auto initial_solution = runner.bind(instance).initial_solution();

        auto solver = make_solver<easylocal::solver::two_stage>(
            std::move(runner),
            easylocal::solver::TwoStageConfig<easylocal::initialization::Initial>{
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
        std::cout << "termination: "
                  << (result.termination == FirstImprovementTermination::local_optimum
                          ? "local optimum"
                          : "evaluation budget exhausted")
                  << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
