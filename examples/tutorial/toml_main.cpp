// The tutorial's Simulated Annealing, configured from a TOML file (ConfigTOML
// component): easylocal_tutorial_toml [file.toml], annealing.toml by default.
#include "tsp.hpp"

#include <easylocal/adapters/toml.hpp>
#include <easylocal/easylocal.hpp>

#include <iostream>
#include <random>

#ifndef EASYLOCAL_TUTORIAL_CONFIG
#define EASYLOCAL_TUTORIAL_CONFIG "annealing.toml"
#endif

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;
    using Classic = runners::temperature::Classic;

    const auto tsp = five_cities();

    // A runner with the default parameters, whose cost has two weights.
    auto sa = el::make_runner<runners::SimulatedAnnealing<Classic>>()
        | (el::solution_manager<TourManager>()
            | el::cost::sum(el::component<TourLength>(), el::component<MaxEdge>()))
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>());

    // [toml] ---------------------------------------------------------------
    el::config::parameter_set configuration;
    configuration.add("solver", sa.configuration()); // paths solver.*

    // Read the file: every key becomes a "path = value" override.
    const auto file =
        el::config::load_toml_file(argc > 1 ? argv[1] : EASYLOCAL_TUTORIAL_CONFIG);
    if (!file)
    {
        // Each error with its line; a value's names its path.
        el::config::print_diagnostics(std::cerr, file);
        return 2;
    }

    // Apply the overrides: all of them, validated, or none.
    const auto applied = el::config::apply_overrides(configuration, file.overrides);
    if (!applied)
    {
        for (const auto& diagnostic : applied.diagnostics)
            std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
        return 2;
    }
    // [toml] ---------------------------------------------------------------

    std::mt19937_64 rng{42};
    auto search = sa.bind(tsp);
    const auto result = search.run(search.initial_solution(), rng);
    std::cout << file.overrides.size() << " values from the file\n"
              << "cost " << result.cost << '\n';
    return 0;
}
