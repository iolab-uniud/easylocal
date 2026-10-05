// Pareto Late Acceptance Hill Climbing, and the front that search_run keeps
// for any runner with a cost::pareto cost (and the solvers merge across their
// runs), on points of a 10 x 10 grid with objectives x + y and (9 - x) + y:
// the front is the row y = 0.
#include <easylocal/app/app.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/pareto_late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers/multi_start.hpp>
#include <easylocal/solvers/pipeline.hpp>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <type_traits>

namespace
{

using namespace easylocal;
using namespace easylocal::runners;

struct Grid
{
    static constexpr int side = 10;
};

struct Point
{
    int x{};
    int y{};

    friend auto operator==(const Point&, const Point&) -> bool = default;
};

// One step in one of the four directions.
struct Step
{
    int dx{};
    int dy{};
};

class PointManager
{
public:
    using input_type = Grid;
    using solution_type = Point;

    explicit PointManager(const Grid& grid) noexcept : grid_{grid} {}

    [[nodiscard]] auto input() const noexcept -> const Grid&
    {
        return grid_;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_solution(RNG& rng) -> Point
    {
        std::uniform_int_distribution<int> coordinate{0, Grid::side - 1};
        const auto x = coordinate(rng);
        return Point{x, coordinate(rng)};
    }

    [[nodiscard]] static auto is_valid(const Point& point) noexcept -> bool
    {
        return point.x >= 0 && point.x < Grid::side && point.y >= 0
            && point.y < Grid::side;
    }

private:
    const Grid& grid_;
};

class StepNeighborhood
{
public:
    using input_type = Grid;
    using solution_type = Point;
    using move_type = Step;

    explicit StepNeighborhood(const PointManager& manager) noexcept
        : grid_{manager.input()}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const Grid&
    {
        return grid_;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Point& point, RNG& rng)
        -> std::optional<Step>
    {
        static constexpr std::array<Step, 4> steps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
        std::uniform_int_distribution<std::size_t> pick{0, steps.size() - 1};
        for (;;)
        {
            const auto step = steps[pick(rng)];
            if (is_valid(point, step))
                return step;
        }
    }

    [[nodiscard]] static auto is_valid(const Point& point, const Step& step) noexcept
        -> bool
    {
        return PointManager::is_valid(Point{point.x + step.dx, point.y + step.dy});
    }

    static void make_move(Point& point, const Step& step) noexcept
    {
        point.x += step.dx;
        point.y += step.dy;
    }

private:
    const Grid& grid_;
};

struct Left
{
    [[nodiscard]] static auto evaluate(const Point& point) noexcept -> int
    {
        return point.x + point.y;
    }
};

struct Right
{
    [[nodiscard]] static auto evaluate(const Point& point) noexcept -> int
    {
        return (Grid::side - 1 - point.x) + point.y;
    }
};

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

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
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
            solvers::MultiStartConfig{.parameters = {.starts = 10}, .seed = 3}};
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
