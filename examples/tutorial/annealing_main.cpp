// The advanced tutorial: scheduled and rejection-triggered reheating.
#include "reheat_on_rejection.hpp"
#include "tsp.hpp"

#include <easylocal/easylocal.hpp>

#include <array>
#include <iostream>
#include <random>

int main()
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;

    // [annealing-problem]
    const auto tsp = five_cities();
    const auto sm =
        el::solution_manager<TourManager>().with_cost(el::component<TourLength>());
    const auto nhe =
        el::neighborhood<TwoOptExplorer>().with_delta<TourLength, TwoOptLengthDelta>();
    // [annealing-problem]

    // [scheduled-reheating]
    using Reheated = runners::temperature::Reheating<runners::temperature::Classic>;
    auto scheduled = el::make_runner<runners::SimulatedAnnealing<Reheated>>(
        {
            .temperature =
                {
                    .descent =
                        {
                            .initial_temperature = 10.0,
                            .final_temperature = 0.1,
                            .cooling_rate = 0.9,
                            .samples_per_temperature = 20,
                        },
                    .allowed_reheats = 2,
                    .reheat_ratio = 0.5,
                },
            .max_evaluations = 5000,
        })
                         .with_solution_manager(sm)
                         .with_neighborhood(nhe);
    // [scheduled-reheating]

    // [custom-reheating]
    using CustomAnnealing = runners::SimulatedAnnealing<ReheatOnRejection>;
    auto custom = el::make_runner<CustomAnnealing>(
        {
            .temperature =
                {
                    .descent =
                        {
                            .initial_temperature = 10.0,
                            .final_temperature = 0.1,
                            .cooling_rate = 0.9,
                            .samples_per_temperature = 20,
                        },
                    .rejections_before_reheat = 10,
                    .allowed_reheats = 2,
                },
            .max_evaluations = 5000,
        })
                      .with_solution_manager(sm)
                      .with_neighborhood(nhe);

    std::mt19937_64 rng{42};
    auto search = custom.bind(tsp);
    const auto result = search.run(search.initial_solution(), rng);
    // [custom-reheating]

    // The same seed and starting tour, for a separate scheduled-reheating run.
    rng.seed(42);
    auto scheduled_search = scheduled.bind(tsp);
    const auto scheduled_result =
        scheduled_search.run(scheduled_search.initial_solution(), rng);
    std::cout << "scheduled reheating " << scheduled_result.cost << '\n'
              << "rejection reheating " << result.cost << '\n';

    // [reheating-app]
    auto application =
        el::app("tsp-reheating")
            .with_solution_manager(sm)
            .with_neighborhood(nhe)
            .with_runner<CustomAnnealing>("reheat", custom.parameters());
    el::Session session{application, tsp, 42};
    const std::array changes{
        el::config::text_override{
            "runners.reheat.temperature.rejections_before_reheat",
            "20"},
        el::config::text_override{"runners.reheat.temperature.allowed_reheats", "3"},
    };
    const auto configured = session.configure(changes);
    if (!configured)
        return 1;
    session.use_initial_solution();
    if (!session.run("reheat"))
        return 1;
    // [reheating-app]
}
