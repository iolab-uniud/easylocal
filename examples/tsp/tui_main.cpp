// The TSP in the interactive terminal tester: a launcher with the 2-opt and
// the swap app, which share the instance and the current solution.
#include "apps.hpp"

#include <easylocal/adapters/tui/launcher.hpp>

#ifndef EASYLOCAL_TSP_INSTANCE_FILE
#error "EASYLOCAL_TSP_INSTANCE_FILE must name the example instance"
#endif

#ifndef EASYLOCAL_TSP_SOLUTION_FILE
#error "EASYLOCAL_TSP_SOLUTION_FILE must name the example solution"
#endif

int main()
{
    // [launcher] -----------------------------------------------------------
    easylocal::tui::run_launcher(
        {
            .title = "EasyLocal TSP Tester",
            .tester =
                {
                    .seed = 0,
                    .input_path = EASYLOCAL_TSP_INSTANCE_FILE,
                    .solution_path = EASYLOCAL_TSP_SOLUTION_FILE,
                },
        },
        tsp::two_opt_app(),
        tsp::swap_app());
    // [launcher] -----------------------------------------------------------
}
