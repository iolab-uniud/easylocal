// Component contract checks for the tutorial's running example.
#include "tsp.hpp"

#include <easylocal/testing.hpp>

int main()
{
    // [fixture]
    namespace elt = easylocal::testing;
    const elt::fixture<tutorial::TourManager> tsp{
        tutorial::five_cities(),
        tutorial::Tour{{0, 1, 2, 3, 4}},
    };
    // [fixture]

    // [run-checks]
    using namespace tutorial;
    return elt::run_checks(
        elt::check_solution_manager(tsp),
        elt::check_cost_component<TourLength>(tsp),
        elt::check_neighborhood<SwapExplorer>(tsp),
        elt::check_neighborhood<TwoOptExplorer>(tsp),
        elt::check_delta_evaluator<TwoOptExplorer, TourLength, TwoOptLengthDelta>(tsp),
        // No delta cost component: the check uses the component's own delta_evaluate.
        elt::check_delta_evaluator<TwoOptExplorer, TourLengthWithDelta>(tsp));
    // [run-checks]
}
