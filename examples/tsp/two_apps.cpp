#include "apps.hpp"
#include "instance_io.hpp"

#include <iostream>

#ifndef EASYLOCAL_TSP_MWE_INSTANCE_FILE
#error "EASYLOCAL_TSP_MWE_INSTANCE_FILE must name the example instance"
#endif

int main()
{
    using easylocal::runners::FirstImprovement;

    auto two_opt = tsp::two_opt_app();
    auto swap = tsp::swap_app();

    const auto instance = tsp::load_instance(EASYLOCAL_TSP_MWE_INSTANCE_FILE);

    // The 2-opt local optimum is the starting point of the swap search.
    auto two_opt_runtime = two_opt.for_input(instance);
    const auto first = two_opt_runtime.run<FirstImprovement>(
        two_opt_runtime.solution_manager().initial_solution());

    auto swap_runtime = swap.for_input(instance);
    const auto second = swap_runtime.run<FirstImprovement>(first.solution);

    std::cout << "two-opt cost: " << first.cost << '\n';
    std::cout << "swap cost:    " << second.cost << '\n';
    return 0;
}
