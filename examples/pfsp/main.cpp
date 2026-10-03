#include "instance.hpp"
#include "makespan_component.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"

#include <easylocal/app/io.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/config/cli.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/tabu_search.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>

#ifndef EASYLOCAL_PFSP_INSTANCE_FILE
#error "EASYLOCAL_PFSP_INSTANCE_FILE must name the example instance"
#endif

namespace
{

struct AppParameters
{
    std::filesystem::path instance_file;
    std::uint64_t seed{2026U};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"instance_file", &AppParameters::instance_file>(
                "PFSP instance file"),
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

} // namespace

int main(int argc, char* argv[])
{
    using namespace pfsp;
    using easylocal::runners::TabuSearch;

    try
    {
        AppParameters app_parameters{.instance_file = EASYLOCAL_PFSP_INSTANCE_FILE};

        // The tabu search of the tabu list study: a fixed-length list, aspiration
        // by objective, exhaustive exploration and a stop after idle iterations.
        auto runner =
            easylocal::make_runner<TabuSearch<>>(
                {.max_idle_iterations = 1000, .tabu_list = {.tenure = 10}})
            | (easylocal::solution_manager<PfspSolutionManager>()
                | easylocal::component<MakespanComponent>())
            | easylocal::neighborhood<SwapJobsNeighborhoodExplorer>();

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
            easylocal::load_input<PfspInstance>(app_parameters.instance_file);
        auto search = runner.bind(instance);
        std::mt19937_64 rng{app_parameters.seed};
        // The search starts from a random schedule, as in the study.
        const auto initial = search.random_solution(rng);

        using cost_type = decltype(search)::cost_type;
        const auto result = run_parameters.target.empty()
            ? search.run(initial, rng)
            : search.run(
                  initial,
                  rng,
                  easylocal::stop_at(
                      easylocal::read_cost<cost_type>(instance, run_parameters.target)));

        std::cout << "instance:    " << app_parameters.instance_file << " ("
                  << instance.describe() << ")\n";
        std::cout << "initial:     " << initial.describe() << '\n';
        std::cout << "best:        " << result.solution.describe() << '\n';
        std::cout << "makespan:    " << result.cost << '\n';
        std::cout << "iterations:  " << result.iterations << '\n';
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
