// Pareto Late Acceptance Hill Climbing, and the front that search_run keeps
// for any runner with a cost::pareto cost (and the solvers merge across their
// runs), on points of a 10 x 10 grid with objectives x + y and (9 - x) + y:
// the front is the row y = 0.
#include "support/expect.hpp"
#include "support/pareto_grid.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/pareto_late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers/multi_start.hpp>
#include <easylocal/solvers/pipeline.hpp>
#include <easylocal/trace/memory_recorder.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <optional>
#include <random>
#include <type_traits>
#include <variant>

namespace
{

using namespace easylocal;
using namespace easylocal::runners;
using namespace pareto_grid;

// The column x of a point, and its mirror: every point of a column has the
// same cost, a plateau of 10 points, and every column is on the front.
struct Column
{
    [[nodiscard]] static auto evaluate(const Point& point) noexcept -> int
    {
        return point.x;
    }
};

struct MirroredColumn
{
    [[nodiscard]] static auto evaluate(const Point& point) noexcept -> int
    {
        return Grid::side - 1 - point.x;
    }
};

// Both objectives maximized: the front is the row y = 9.
struct Maximized
{
    [[nodiscard]] auto operator()(const cost::pareto<int, int>& cost) const
        -> cost::pareto<int, int>
    {
        return cost;
    }

    [[nodiscard]] auto compare(
        const cost::pareto<int, int>& lhs,
        const cost::pareto<int, int>& rhs) const -> std::partial_ordering
    {
        return rhs <=> lhs;
    }
};

template<class Algorithm>
[[nodiscard]]
auto grid_runner(const typename Algorithm::parameters_type parameters)
{
    return easylocal::make_runner<Algorithm>(parameters)
        | (solution_manager<PointManager>()
            | cost::objectives(component<Left>(), component<Right>()))
        | neighborhood<StepNeighborhood>();
}

// The grid's steps, none proposed: every point is a local optimum.
class StuckNeighborhood : public StepNeighborhood
{
public:
    using StepNeighborhood::StepNeighborhood;

    template<std::uniform_random_bit_generator RNG>
    static std::optional<Step> random_move(const Point&, RNG&)
    {
        return std::nullopt;
    }
};

// The history's random point is always (5, 0), and the only move is one step
// right, which worsens both objectives of Left and Column on the row y = 0:
// only a second chance accepts a move.
class FixedHistoryManager : public PointManager
{
public:
    using PointManager::PointManager;

    template<std::uniform_random_bit_generator RNG>
    static Point random_solution(RNG&)
    {
        return Point{5, 0};
    }
};

class RightStepNeighborhood : public StepNeighborhood
{
public:
    explicit RightStepNeighborhood(const FixedHistoryManager& manager) noexcept
        : StepNeighborhood{manager}
    {
    }

    template<std::uniform_random_bit_generator RNG>
    static std::optional<Step> random_move(const Point& point, RNG&)
    {
        const Step right{1, 0};
        return is_valid(point, right) ? std::optional<Step>{right} : std::nullopt;
    }
};

// Every point of the front is on the row y = 0, none dominates another, and
// they are ordered by the first objective.
template<class Front>
[[nodiscard]]
auto valid_front(const Front& front) -> bool
{
    for (std::size_t index = 0; index < front.size(); ++index)
    {
        if (front[index].solution.y != 0)
            return false;
        if (index > 0
            && !(
                front[index - 1].cost.template get<0>()
                < front[index].cost.template get<0>()))
            return false;
    }
    return !front.empty();
}

} // namespace

int main()
{
    bool ok = true;
    const Grid grid;

    {
        ok &= expect(
            !ParetoLateAcceptanceHillClimbingParameters{.history_length = 0}.validate()
                && !ParetoLateAcceptanceHillClimbingParameters{.idle_ratio = 1.5}
                    .validate()
                && static_cast<bool>(
                    ParetoLateAcceptanceHillClimbingParameters{}.validate()),
            "pareto late acceptance validates its parameters");
        ParetoLateAcceptanceHillClimbingParameters parameters;
        config::parameter_set configuration;
        configuration.add(parameters);
        const std::array overrides{config::text_override{"second_chance", "false"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(overrides))
                && !parameters.second_chance,
            "pareto late acceptance exposes its parameters");
    }

    {
        // The initial point is far from the front; the history adds random
        // points, and the search brings them down to the row y = 0.
        std::mt19937 rng{11U};
        const auto result =
            grid_runner<ParetoLateAcceptanceHillClimbing>(
                {.history_length = 10, .min_iterations = 2000})
                .bind(grid)
                .run(Point{4, 9}, rng);
        // A Pareto result is a search_result with a front.
        static_assert(std::derived_from<
            std::remove_cvref_t<decltype(result)>,
            search_result<Point, cost::pareto<int, int>>>);
        static_assert(std::same_as<
            std::remove_cvref_t<decltype(result.front)>,
            std::vector<pareto_point<Point, cost::pareto<int, int>>>>);
        ok &= expect(valid_front(result.front), "the front is on the row y = 0, ordered");
        ok &= expect(
            result.front.size() >= 5,
            "the history spreads the front over several trade-offs");
        ok &= expect(
            result.solution == result.front.front().solution
                && result.cost == result.front.front().cost,
            "the result's solution is the first of the front");
        ok &= expect(
            result.termination == termination_reason::idle_limit_reached
                && result.iterations >= 2000,
            "past min_iterations the search stops when mostly idle");
    }

    {
        // From (3, 0) the step to (4, 0) is worse than the current point but
        // better than the next one of the history, (5, 0): the second chance
        // takes it, once; without it no move is accepted.
        const auto accepted = [&](const bool second_chance) {
            std::mt19937 rng{11U};
            trace::memory_recorder<cost::pareto<int, int>> recorder;
            static_cast<void>((
                easylocal::make_runner<ParetoLateAcceptanceHillClimbing>(
                    {.history_length = 2,
                        .min_iterations = 5,
                        .idle_ratio = 0.5,
                        .second_chance = second_chance})
                | (solution_manager<FixedHistoryManager>()
                    | cost::objectives(component<Left>(), component<Column>()))
                | neighborhood<RightStepNeighborhood>())
                    .bind(grid)
                    .run(Point{3, 0}, rng, with(recorder)));
            std::size_t count = 0;
            for (const auto& record : recorder.records())
                count += std::holds_alternative<
                    trace::memory_recorder<cost::pareto<int, int>>::move_accepted_record>(
                    record);
            return count;
        };
        ok &= expect(
            accepted(true) == 1 && accepted(false) == 0,
            "the second chance accepts a move that dominates the next solution");
    }

    {
        // Without a move to try the search ends at once, a local optimum.
        std::mt19937 rng{11U};
        const auto result =
            (easylocal::make_runner<ParetoLateAcceptanceHillClimbing>(
                 {.history_length = 3})
                | (solution_manager<PointManager>()
                    | cost::objectives(component<Left>(), component<Right>()))
                | neighborhood<StuckNeighborhood>())
                .bind(grid)
                .run(Point{4, 9}, rng);
        ok &= expect(
            result.termination == termination_reason::local_optimum
                && result.iterations == 0 && result.evaluations == 3,
            "pareto late acceptance ends at a solution without moves");
    }

    {
        std::mt19937 rng{11U};
        const auto result =
            grid_runner<ParetoLateAcceptanceHillClimbing>(
                {.history_length = 10, .min_iterations = 2000, .second_chance = false})
                .bind(grid)
                .run(Point{4, 9}, rng);
        ok &= expect(valid_front(result.front), "without the second chance too");
    }

    {
        std::mt19937 rng{11U};
        const auto result =
            grid_runner<ParetoLateAcceptanceHillClimbing>(
                {.history_length = 10, .max_evaluations = 25})
                .bind(grid)
                .run(Point{4, 9}, rng);
        ok &= expect(
            result.evaluations == 25
                && result.termination == termination_reason::evaluation_budget_exhausted
                && !result.front.empty(),
            "the history's evaluations count towards the budget");
    }

    {
        // The target (6, 5) is met only around (5, 0), while the first point of
        // the front is (0, 9): the run returns a point of its front that meets
        // the target it reports as reached.
        const cost::pareto<int, int> target{6, 5};
        std::mt19937 rng{11U};
        const auto result =
            grid_runner<ParetoLateAcceptanceHillClimbing>(
                {.history_length = 10, .min_iterations = 2000})
                .bind(grid)
                .run(Point{4, 9}, rng, stop_at(target));
        ok &= expect(
            result.termination == termination_reason::target_reached
                && result.cost.get<0>() <= 6 && result.cost.get<1>() <= 5,
            "a run that reaches a pareto target returns a solution that meets it");
    }

    {
        // A cost that meets the target is kept by best_so_far even when it does
        // not dominate the best one.
        struct TargetRun
        {
            cost::pareto<int, int> goal{6, 5};

            [[nodiscard]] static auto better(
                const cost::pareto<int, int>& lhs,
                const cost::pareto<int, int>& rhs) -> bool
            {
                return lhs < rhs;
            }

            [[nodiscard]] static auto better_or_equivalent(
                const cost::pareto<int, int>& lhs,
                const cost::pareto<int, int>& rhs) -> bool
            {
                return lhs <= rhs;
            }

            [[nodiscard]] auto target() const -> const cost::pareto<int, int>*
            {
                return &goal;
            }

            static void incumbent_updated(
                const cost::pareto<int, int>&,
                const cost::pareto<int, int>&)
            {
            }
        };
        struct Evaluated
        {
            cost::pareto<int, int> value;

            [[nodiscard]] auto cost() const -> const cost::pareto<int, int>&
            {
                return value;
            }
        };
        TargetRun run;
        best_so_far best{Point{0, 0}, cost::pareto<int, int>{0, 9}};
        const auto kept =
            best.update(run, Point{5, 0}, Evaluated{cost::pareto<int, int>{5, 4}});
        const auto ignored =
            best.update(run, Point{6, 0}, Evaluated{cost::pareto<int, int>{6, 3}});
        ok &= expect(
            kept && !ignored && best.solution == Point{5, 0},
            "best_so_far keeps the first cost that meets the target");
    }

    {
        // Any runner with a pareto cost gets the front of the solutions it
        // reaches: Hill Climbing goes down to the row y = 0 from (4, 9).
        std::mt19937 rng{11U};
        const auto result =
            grid_runner<HillClimbing>({.max_idle_iterations = 50})
                .bind(grid)
                .run(Point{4, 9}, rng);
        ok &= expect(
            !result.front.empty() && result.front.front().solution.y == 0
                && result.solution.y == 0,
            "hill climbing with a pareto cost returns its front");
    }

    {
        // Hill Climbing reaches one point of the row y = 0 from each start:
        // MultiStart and the attempts of a stage merge the fronts of their
        // runs.
        solvers::MultiStart solver{
            grid_runner<HillClimbing>({.max_idle_iterations = 50}),
            {.starts = 10}};
        solver.seed(3);
        const auto result = solver.solve(grid);
        ok &= expect(
            valid_front(result.front) && result.front.size() >= 3,
            "MultiStart merges the fronts of its starts");

        auto descent = grid_runner<HillClimbing>({.max_idle_iterations = 50});
        auto pipeline =
            solvers::pipeline(solvers::stage("hc", descent) & solvers::attempts(10))
                .seed(3);
        const auto staged = pipeline.solve(grid);
        ok &= expect(
            valid_front(staged.front) && staged.front.size() >= 3,
            "the attempts of a stage merge their fronts");
    }

    {
        // The archive compares with the run's relations: with a compare that
        // maximizes both objectives, the front is the row y = 9.
        std::mt19937 rng{11U};
        const auto result =
            (easylocal::make_runner<ParetoLateAcceptanceHillClimbing>(
                 {.history_length = 10, .min_iterations = 2000})
                | (solution_manager<PointManager>()
                    | cost::apply(
                        Maximized{},
                        cost::objectives(component<Left>(), component<Right>())))
                | neighborhood<StepNeighborhood>())
                .bind(grid)
                .run(Point{4, 0}, rng);
        ok &= expect(
            !result.front.empty()
                && std::ranges::all_of(
                    result.front,
                    [](const auto& point) { return point.solution.y == 9; })
                && result.solution.y == 9,
            "a maximizing compare gives the front of the largest costs");
    }

    {
        // On a plateau the archive keeps one point per cost by default; every
        // distinct solution only on request, up to max_front_size.
        auto climber = easylocal::make_runner<HillClimbing>({.max_idle_iterations = 500})
            | (solution_manager<PointManager>()
                | cost::objectives(component<Column>(), component<MirroredColumn>()))
            | neighborhood<StepNeighborhood>();
        auto bound = climber.bind(grid);
        std::mt19937 rng{5U};
        const auto one = bound.run(Point{4, 4}, rng);
        ok &= expect(one.front.size() == 1, "one point per cost on a plateau");

        const auto kept = bound.run(
            Point{4, 4},
            rng,
            run_options<trace::null_tracer>{}.keep_front(
                {.keep_equivalent = true, .max_front_size = 6}));
        ok &= expect(
            kept.front.size() == 6
                && std::ranges::all_of(
                    kept.front,
                    [](const auto& point) { return point.solution.x == 4; }),
            "keep_equivalent keeps distinct solutions of a cost, up to the bound");
    }

    {
        // An app carries the front of the runner chosen by name.
        auto application =
            easylocal::app("grid")
                .with_solution_manager(
                    solution_manager<PointManager>()
                    | cost::objectives(component<Left>(), component<Right>()))
                .with_neighborhood(neighborhood<StepNeighborhood>())
                .with_runner<ParetoLateAcceptanceHillClimbing>("plahc");
        std::mt19937 rng{11U};
        const auto result = application.run("plahc", grid, Point{4, 9}, rng);
        ok &= expect(
            result.has_value() && valid_front(result->front) && result->front.size() >= 5
                && result->solution == result->front.front().solution,
            "a run by name returns the front");
    }

    return ok ? 0 : 1;
}
