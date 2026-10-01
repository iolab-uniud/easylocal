// The tutorial's TSP in the interactive terminal tester (TUI component).
#include "tsp.hpp"

#include <easylocal/adapters/tui.hpp>
#include <easylocal/easylocal.hpp>

#include <utility>

int main()
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;
    using Classic = runners::temperature::Classic;

    // [tui] ----------------------------------------------------------------
    auto application = el::app("tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::SimulatedAnnealing<Classic>>("sa");

    el::Tester tester{std::move(application)};
    tester.set_input(five_cities());

    el::tui::run(
        tester,
        {
            .title = "TSP tester",
            .seed = 2026, // the RNG for random solutions, moves and stochastic runners
        });
    // [tui] ----------------------------------------------------------------
}
