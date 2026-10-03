#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/config/cli.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>

#ifndef EASYLOCAL_TSP_MWE_INSTANCE_FILE
#error "EASYLOCAL_TSP_MWE_INSTANCE_FILE must name the example instance"
#endif

namespace
{

using namespace tsp;

struct AppParameters
{
    std::filesystem::path instance_file;
    std::uint64_t seed{2026U};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"instance_file", &AppParameters::instance_file>(
                "TSP instance file"),
            easylocal::config::field<"seed", &AppParameters::seed>(
                "Pseudo-random generator seed"));
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

void print_tour(const Tour& solution)
{
    std::cout << '[';

    for (std::size_t position = 0; position < solution.tour.size(); ++position)
    {
        if (position != 0)
            std::cout << ", ";

        std::cout << solution.tour[position];
    }

    std::cout << ']';
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace tsp;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::neighborhood_union;
    using easylocal::random_biases;
    using easylocal::solution_manager;
    using easylocal::runners::SimulatedAnnealing;
    using easylocal::runners::temperature::FixedLength;
    using easylocal::runners::temperature::FixedLengthParameters;

    try
    {
        AppParameters app_parameters{
            .instance_file = EASYLOCAL_TSP_MWE_INSTANCE_FILE,
        };
        FixedLengthParameters temperature_parameters{
            .initial_temperature = 8.0,
            .final_temperature = 0.25,
            .cooling_rate = 0.75,
            .max_iterations = 200,
        };
        // TourLengthValue is a domain value: TourLengthCost maps it to the
        // scalar cost explicitly.
        auto runner =
            easylocal::make_runner<SimulatedAnnealing<FixedLength>>(
                {.temperature = temperature_parameters})
            | (solution_manager<TspSolutionManager>()
                | easylocal::cost::apply(
                    TourLengthCost{},
                    component<TourLengthComponent>()))
            | (neighborhood_union(
                   neighborhood<TwoOptNeighborhoodExplorer>()
                       | delta<TourLengthComponent, TwoOptTourLengthDeltaEvaluator>(),
                   neighborhood<SwapCitiesNeighborhoodExplorer>()
                       | delta<TourLengthComponent, SwapTourLengthDeltaEvaluator>())
                | random_biases(3.0, 1.0));

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
        auto search = runner.bind(instance);
        const auto initial_solution = search.initial_solution();

        std::mt19937_64 rng{app_parameters.seed};
        const auto result = search.run(initial_solution, rng);

        std::cout << "instance:     " << app_parameters.instance_file << '\n';
        std::cout << "initial tour: ";
        print_tour(initial_solution);
        std::cout << '\n';

        std::cout << "best tour:    ";
        print_tour(result.solution);
        std::cout << '\n';

        std::cout << "best length: " << result.cost << '\n';
        std::cout << "iterations: " << result.iterations << '\n';
        std::cout << "evaluations: " << result.evaluations << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
