// A runner registered with its own neighborhood: built over the app's
// SolutionManager, next to the app's neighborhood, run by name, configured
// under runners.<name>.neighborhood and checked by check(app).
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/check.hpp>
#include <easylocal/app/io.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <cassert>
#include <cstddef>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#ifndef EASYLOCAL_TUTORIAL_INSTANCE
#error "EASYLOCAL_TUTORIAL_INSTANCE must name the tutorial's instance"
#endif

namespace
{

using namespace tutorial;
namespace el = easylocal;
using el::runners::BestImprovement;
using el::runners::FirstImprovement;

struct WindowParameters
{
    std::size_t window{1};

    static consteval auto parameter_schema()
    {
        return el::config::fields(
            el::config::field<"window", &WindowParameters::window>(
                "The largest distance between the swapped positions",
                el::config::range(std::size_t{1}, std::size_t{100})));
    }

    el::config::validation_result validate() const
    {
        if (window == 0)
            return el::config::validation_result::failure("window must be positive");
        return el::config::validation_result::success();
    }
};

// Swaps of two positions at most window apart: a neighborhood with parameters.
class WindowSwapExplorer : public el::neighborhood_explorer_base<TourManager, SwapCities>
{
public:
    using parameters_type = WindowParameters;

    WindowSwapExplorer(const TourManager& manager, const WindowParameters& parameters)
        : neighborhood_explorer_base{manager}, window_{parameters.window}
    {
    }

    el::generator<SwapCities> moves(const Tour& tour) const
    {
        const auto n = tour.order.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n && j - i <= window_; ++j)
                co_yield SwapCities{i, j};
    }

    bool is_valid(const Tour& tour, const SwapCities& move) const
    {
        return move.i < move.j && move.j < tour.order.size()
            && move.j - move.i <= window_;
    }

    void make_move(Tour& tour, const SwapCities& move) const
    {
        std::swap(tour.order[move.i], tour.order[move.j]);
    }

private:
    std::size_t window_;
};

auto solution_manager()
{
    return el::solution_manager<TourManager>() | el::component<TourLength>();
}

auto tsp_app()
{
    return el::app("tsp") | solution_manager()
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<FirstImprovement>("two-opt")
        | el::runner<BestImprovement>("swap", {}, el::neighborhood<WindowSwapExplorer>());
}

bool has(const el::config::parameter_set& parameters, const std::string_view path)
{
    for (const auto& parameter : parameters.parameters())
        if (parameter.path == path)
            return true;
    return false;
}

} // namespace

int main()
{
    const auto input = el::load_input<Tsp>(EASYLOCAL_TUTORIAL_INSTANCE);
    auto application = tsp_app();
    assert(
        (application.runner_names() == std::vector<std::string_view>{"two-opt", "swap"}));

    // The bound app builds the runner's neighborhood over its SolutionManager.
    {
        auto bound = application.bind(input);
        std::size_t own = 0;
        el::detail::app_access::for_each_own_neighborhood(
            bound,
            [&](const std::string_view name, const WindowSwapExplorer& neighborhood) {
                assert(name == "swap");
                assert(&neighborhood.input() == &bound.input());
                ++own;
            });
        assert(own == 1);
        const auto swap = el::detail::app_access::runner<BestImprovement>(bound);
        assert(&swap.solution_manager() == &bound.solution_manager());
    }

    // Run by name, the runner explores its own neighborhood: the same search
    // as a standalone runner on it.
    const auto start = TourManager{input}.initial_solution();
    std::mt19937_64 rng{0};
    const auto by_name = application.run("swap", input, start, rng);
    assert(by_name);
    auto standalone = el::make_runner<BestImprovement>() | solution_manager()
        | el::neighborhood<WindowSwapExplorer>();
    auto bound_runner = standalone.bind(input);
    const auto direct = bound_runner.run(start);
    assert(by_name->solution.order == direct.solution.order);
    assert(by_name->evaluations == direct.evaluations);

    // make_runner gives the same runner, with its neighborhood.
    auto made = application.make_runner<BestImprovement>("swap");
    auto made_bound = made.bind(input);
    assert(made_bound.run(start).solution.order == direct.solution.order);

    // Its parameters are under runners.<name>.neighborhood, read at each run.
    auto parameters = application.configuration();
    assert(has(parameters, "runners.swap.neighborhood.window"));
    assert(has(parameters, "neighborhood") == false);
    const auto overridden = el::config::apply_overrides(
        parameters,
        std::vector<el::config::text_override>{
            {"runners.swap.neighborhood.window", "4"}});
    assert(overridden);
    const auto wider = application.run("swap", input, start, rng);
    assert(wider && wider->evaluations > by_name->evaluations);

    // check(app) checks both neighborhoods.
    const auto report = el::check(application, input);
    assert(report.passed());
    assert(report.composition().neighborhoods == 2);

    // A session runs it like any runner; its moves are the app's.
    el::Session session{application, input, 1};
    session.use_initial_solution();
    assert(session.run("swap"));
    assert(session.solution().order == wider->solution.order);
    return 0;
}
