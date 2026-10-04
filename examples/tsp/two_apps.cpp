// Two searches in a row on the TSP: a first improvement with 2-opt moves, then
// one with swaps from the 2-opt local optimum.
#include "apps.hpp"

#include <easylocal/app/io.hpp>

#include <iostream>

#ifndef EASYLOCAL_TSP_INSTANCE_FILE
#error "EASYLOCAL_TSP_INSTANCE_FILE must name the example instance"
#endif

int main()
{
    using easylocal::runners::FirstImprovement;

    const auto instance =
        easylocal::load_input<tsp::TspInstance>(EASYLOCAL_TSP_INSTANCE_FILE);
    const auto start = tsp::TspSolutionManager{instance}.initial_solution();

    const auto first = tsp::two_opt_app().run<FirstImprovement>(instance, start);
    const auto second = tsp::swap_app().run<FirstImprovement>(instance, first.solution);

    std::cout << "two-opt cost: " << first.cost << '\n';
    std::cout << "swap cost:    " << second.cost << '\n';
    return 0;
}
