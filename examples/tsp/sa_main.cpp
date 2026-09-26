#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"
#include "swap_tour_length_delta.hpp"
#include "tour_length_delta.hpp"

#include <easylocal/aggregation.hpp>
#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/neighborhood_union.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/simulated_annealing.hpp>
#include <easylocal/search/temperature_policy.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifndef EASYLOCAL_TSP_MWE_INSTANCE_FILE
#error "EASYLOCAL_TSP_MWE_INSTANCE_FILE must name the example instance"
#endif

namespace
{

using namespace easylocal::mwe::tsp;

struct AppParameters
{
    std::filesystem::path instance_file;
    std::uint64_t seed{2026U};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "instance_file",
                &AppParameters::instance_file>(
                    "TSP instance file"),
            easylocal::config::field<
                "seed",
                &AppParameters::seed>(
                    "Pseudo-random generator seed"));
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

template<class Parameters>
void require_valid(const Parameters& parameters)
{
    const auto validation = parameters.validate();
    if (!validation)
    {
        throw std::invalid_argument{std::string{validation.message}};
    }
}

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

void print_tour(const Tour& solution)
{
    std::cout << '[';

    for (std::size_t position = 0; position < solution.tour.size(); ++position)
    {
        if (position != 0)
        {
            std::cout << ", ";
        }

        std::cout << solution.tour[position];
    }

    std::cout << ']';
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace easylocal::mwe::tsp;
    using easylocal::NeighborhoodUnionParameters;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::neighborhood_union;
    using easylocal::random_biases;
    using easylocal::solution_manager;
    using easylocal::search::SimulatedAnnealing;
    using easylocal::search::temperature::FixedLength;
    using easylocal::search::temperature::FixedLengthParameters;

    try
    {
        AppParameters app_parameters{
            .instance_file = EASYLOCAL_TSP_MWE_INSTANCE_FILE,
            .seed = 2026U,
        };
        FixedLengthParameters temperature_parameters{
            .initial_temperature = 8.0,
            .final_temperature = 0.25,
            .cooling_rate = 0.75,
            .max_iterations = 200,
        };
        NeighborhoodUnionParameters<2> neighborhood_parameters{
            .random_biases = {3.0, 1.0},
        };

        require_valid(app_parameters);
        require_valid(temperature_parameters);
        require_valid(neighborhood_parameters);

        // Equivalent tag-based spelling:
        // auto runner = make_runner<easylocal::runner::simulated_annealing>(
        //     FixedLength{temperature_parameters}) | ...;
        auto runner =
            Runner{SimulatedAnnealing{FixedLength{temperature_parameters}}}
            | (solution_manager<TspSolutionManager>()
               | component<TourLengthComponent>()
               | easylocal::aggregator(easylocal::aggregation::weighted_sum{distance_type{1}}))
            | (neighborhood_union(
                   neighborhood<TwoOptNeighborhoodExplorer>()
                       | delta<
                             TourLengthComponent,
                             TwoOptTourLengthDeltaEvaluator>(),
                   neighborhood<SwapCitiesNeighborhoodExplorer>()
                       | delta<
                             TourLengthComponent,
                             SwapTourLengthDeltaEvaluator>())
               | random_biases(
                     neighborhood_parameters.random_biases[0],
                     neighborhood_parameters.random_biases[1]));

        const auto configuration = easylocal::config::root(
            easylocal::config::named<"application">(app_parameters),
            runner.configuration<"solver">());

        const auto cli = easylocal::config::parse_cli(argc, argv);
        if (cli.help_requested)
        {
            std::cout << easylocal::config::cli_help(argv[0], configuration);
            return 0;
        }

        easylocal::config::config_file_parse_result file_configuration{};
        if (cli.config_file.has_value())
        {
            file_configuration =
                easylocal::config::load_config_file(*cli.config_file);
        }

        if (!cli || !file_configuration)
        {
            for (const auto& diagnostic : cli.diagnostics)
            {
                std::cerr << "error: " << diagnostic.argument << ": "
                          << diagnostic.message << '\n';
            }
            for (const auto& diagnostic : file_configuration.diagnostics)
            {
                std::cerr << "error: ";
                if (diagnostic.line != 0)
                {
                    std::cerr << "config line " << diagnostic.line << ": ";
                }
                std::cerr << diagnostic.message;
                if (!diagnostic.text.empty())
                {
                    std::cerr << " ('" << diagnostic.text << "')";
                }
                std::cerr << '\n';
            }
            return 2;
        }

        const auto effective_overrides = easylocal::config::overlay_overrides(
            std::span<const easylocal::config::owned_text_override>{
                file_configuration.overrides},
            std::span<const easylocal::config::text_override>{cli.overrides});
        const auto effective_views = easylocal::config::override_views(
            std::span<const easylocal::config::owned_text_override>{
                effective_overrides});
        const auto overrides = easylocal::config::apply_overrides(
            configuration,
            std::span<const easylocal::config::text_override>{effective_views});
        if (!overrides)
        {
            for (const auto& diagnostic : overrides.diagnostics)
            {
                std::cerr << "error: " << diagnostic.path;
                if (!diagnostic.value.empty())
                {
                    std::cerr << " = '" << diagnostic.value << '\'';
                }
                std::cerr << ": " << diagnostic.message << '\n';
            }
            return 2;
        }

        print_configuration(configuration);

        const auto instance = load_instance(app_parameters.instance_file);
        const Tour initial_solution{
            .tour = {0, 2, 4, 1, 5, 3},
        };

        std::mt19937 rng{static_cast<std::mt19937::result_type>(
            app_parameters.seed)};
        const auto result = runner.bind(instance).run(initial_solution, rng);

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
