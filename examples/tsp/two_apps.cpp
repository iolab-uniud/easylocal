// Two searches in a row on the TSP: a first improvement with 2-opt moves, then
// one with swaps from the 2-opt local optimum.
#include "apps.hpp"

#include <easylocal/app/io.hpp>

#include <iostream>
#include <random>

#ifndef EASYLOCAL_TSP_INSTANCE_FILE
#error "EASYLOCAL_TSP_INSTANCE_FILE must name the example instance"
#endif

int main()
{
    const auto instance =
        easylocal::load_input<tsp::TspInstance>(EASYLOCAL_TSP_INSTANCE_FILE);
    const auto start = tsp::TspSolutionManager{instance}.initial_solution();

    // A runner is run by its name, and the result is empty when no runner has
    // it; First Improvement does not use the generator, which a stochastic
    // runner would.
    std::mt19937_64 rng{0};
    const auto first = tsp::two_opt_app().run("fi", instance, start, rng);
    if (!first)
        return 1;
    const auto second = tsp::swap_app().run("fi", instance, first->solution, rng);
    if (!second)
        return 1;

    std::cout << "two-opt cost: " << first->cost << '\n';
    std::cout << "swap cost:    " << second->cost << '\n';
    return 0;
}
