// The tutorial's Simulated Annealing, configured from a TOML file (ConfigTOML
// component): ./easylocal_tutorial_toml annealing.toml
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
    auto sa = el::make_runner<runners::SimulatedAnnealing<Classic>>(Classic{{}})
        | (el::solution_manager<TourManager>()
            | el::cost::sum(el::component<TourLength>(), el::component<MaxEdge>()))
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>());

    // [toml] ---------------------------------------------------------------
    const auto configuration = el::config::root(sa.configuration<"solver">());

    // Read the file: every key becomes a "path = value" override.
    const auto file =
        el::config::load_toml_file(argc > 1 ? argv[1] : EASYLOCAL_TUTORIAL_CONFIG);
    if (!file)
    {
        for (const auto& diagnostic : file.diagnostics)
            std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
        return 2;
    }

    // Apply the overrides to the tree: all of them, validated, or none.
    const auto applied = el::config::apply_overrides(
        configuration,
        el::config::override_views(file.overrides));
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
    std::cout << applied.applied_parameter_blocks << " parameter blocks from the file\n"
              << "cost " << result.cost << '\n';
    return 0;
}
