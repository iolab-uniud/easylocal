// The tutorial's TSP behind the command line of a typical EasyLocal 3 program
// (docs/from-easylocal-3.md): an instance, a seed and a method, which names the runner.
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef EASYLOCAL_TUTORIAL_INSTANCE
#define EASYLOCAL_TUTORIAL_INSTANCE "five.tsp"
#endif

namespace
{

// [migrated-parameters] ----------------------------------------------------
// EasyLocal 3's ParameterBox "main" and its Parameter<T> members, as a
// parameter block: --main.instance, --main.seed, --main.method, ...
struct MainParameters
{
    std::filesystem::path instance;
    std::uint64_t seed{0};
    std::string method{"sa"};
    std::filesystem::path output_file;

    static consteval auto parameter_schema()
    {
        namespace config = easylocal::config;
        return config::fields(
            config::field<"instance", &MainParameters::instance>("Input instance"),
            config::field<"seed", &MainParameters::seed>("Random seed"),
            config::field<"method", &MainParameters::method>("Runner: fi or sa"),
            config::field<"output_file", &MainParameters::output_file>(
                "Solution file (empty: standard output)"));
    }

    // EasyLocal 3 checked IsSet() in main; a block checks its own values.
    easylocal::config::validation_result validate() const
    {
        if (instance.empty())
            return easylocal::config::validation_result::failure("instance must be set");
        return easylocal::config::validation_result::success();
    }
};
// [migrated-parameters] ----------------------------------------------------

} // namespace

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;
    using Classic = runners::temperature::Classic;

    // [migrated-app] -------------------------------------------------------
    // The objects EasyLocal 3 built and linked in main (sm.AddCostComponent,
    // nhe.AddDeltaCostComponent, one runner object each), as one description.
    auto application = el::app("tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::SimulatedAnnealing<Classic>>(
            "sa",
            {.temperature = {.samples_per_temperature = 50}});
    // [migrated-app] -------------------------------------------------------

    // [migrated-command-line] ----------------------------------------------
    MainParameters main_parameters{.instance = EASYLOCAL_TUTORIAL_INSTANCE};
    el::config::parameter_set configuration;
    configuration.add("main", main_parameters);
    configuration.add(application.configuration()); // --runners.sa.temperature.*

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
    // [migrated-command-line] ----------------------------------------------

    // [migrated-run] -------------------------------------------------------
    // SimpleLocalSearch with SetRunner and Solve: a Session on the Input,
    // which runs a registered runner by name from a random solution.
    const auto tsp = el::load_input<Tsp>(main_parameters.instance);
    el::Session session{application, tsp, main_parameters.seed};
    session.use_random_solution(session.rng());
    if (!session.run(main_parameters.method))
    {
        std::cerr << "unknown method " << main_parameters.method << '\n';
        return 2;
    }

    std::cout << "cost " << session.evaluate() << '\n';
    if (main_parameters.output_file.empty())
        el::write_solution(tsp, session.solution(), std::cout);
    else
        el::save_solution(tsp, session.solution(), main_parameters.output_file);
    // [migrated-run] -------------------------------------------------------
    return 0;
}
