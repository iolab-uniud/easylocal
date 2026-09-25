#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "swap_neighborhood_explorer.hpp"

#include <easylocal/neighborhood_union.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/simulated_annealing.hpp>
#include <easylocal/search/temperature_policy.hpp>

#include <cstddef>
#include <iostream>
#include <random>

namespace
{

using namespace easylocal::mwe::tsp;

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

int main()
{
    using namespace easylocal::mwe::tsp;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::neighborhood;
    using easylocal::neighborhood_union;
    using easylocal::random_biases;
    using easylocal::solution_manager;
    using easylocal::search::SimulatedAnnealing;
    using easylocal::search::temperature::FixedLength;
    using easylocal::search::temperature::FixedLengthParameters;

    const TspInstance instance{
        .city_count = 6,
        .distances = {
             0.0, 2.0, 9.0, 10.0, 7.0, 3.0,
             2.0, 0.0, 6.0,  4.0, 3.0, 8.0,
             9.0, 6.0, 0.0,  8.0, 5.0, 7.0,
            10.0, 4.0, 8.0,  0.0, 6.0, 5.0,
             7.0, 3.0, 5.0,  6.0, 0.0, 4.0,
             3.0, 8.0, 7.0,  5.0, 4.0, 0.0,
        },
    };

    const Tour initial_solution{
        .tour = {0, 2, 4, 1, 5, 3},
    };

    auto runner =
        Runner{SimulatedAnnealing{FixedLength{FixedLengthParameters{
            .initial_temperature = 8.0,
            .final_temperature = 0.25,
            .cooling_rate = 0.75,
            .max_iterations = 200,
        }}}}
        | (solution_manager<TspSolutionManager>()
           | component<TourLengthComponent>())
        | (neighborhood_union(
               neighborhood<TwoOptNeighborhoodExplorer>(),
               neighborhood<SwapCitiesNeighborhoodExplorer>())
           | random_biases(3.0, 1.0));

    std::mt19937 rng{2026U};
    const auto result = runner.bind(instance).run(initial_solution, rng);

    std::cout << "initial tour: ";
    print_tour(initial_solution);
    std::cout << '\n';

    std::cout << "best tour:    ";
    print_tour(result.solution);
    std::cout << '\n';

    std::cout << "best length: " << result.cost << '\n';
    std::cout << "iterations: " << result.iterations << '\n';
    std::cout << "evaluations: " << result.evaluations << '\n';

    return 0;
}
