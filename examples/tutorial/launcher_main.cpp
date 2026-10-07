// The tutorial's TSP in a launcher of two apps, one per neighborhood, which
// share the Input and the current solution (TUI component, chapter 13).
#include "tsp.hpp"

#include <easylocal/adapters/tui/launcher.hpp>
#include <easylocal/easylocal.hpp>

#ifndef EASYLOCAL_TUTORIAL_INSTANCE
#define EASYLOCAL_TUTORIAL_INSTANCE "five.tsp"
#endif

namespace
{

namespace el = easylocal;

// [apps] -------------------------------------------------------------------
// The SolutionManager recipe of both apps: the launcher passes the Input and
// the solution from one app to the other, so they must have the same one.
auto tour_manager()
{
    return el::solution_manager<tutorial::TourManager>()
        | el::component<tutorial::TourLength>();
}

// One app per neighborhood. The type of an app spells out all its recipes, so
// the functions let auto deduce it.
auto two_opt_app()
{
    return el::app("tsp-two-opt") | tour_manager()
        | (el::neighborhood<tutorial::TwoOptExplorer>()
            | el::delta<tutorial::TourLength, tutorial::TwoOptLengthDelta>())
        | el::runner<el::runners::FirstImprovement>("fi");
}

auto swap_app()
{
    return el::app("tsp-swap") | tour_manager()
        | el::neighborhood<tutorial::SwapExplorer>()
        | el::runner<el::runners::FirstImprovement>("fi");
}
// [apps] -------------------------------------------------------------------

} // namespace

int main()
{
    // [launcher] -----------------------------------------------------------
    el::tui::run_launcher(
        {
            .title = "TSP launcher",
            .tester =
                {
                    .seed = 2026,
                    .input_path = EASYLOCAL_TUTORIAL_INSTANCE,
                },
        },
        two_opt_app(),
        swap_app());
    // [launcher] -----------------------------------------------------------
}
