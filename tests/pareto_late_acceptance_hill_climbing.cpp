// Pareto Late Acceptance Hill Climbing, and the front that search_run keeps
// for any runner with a cost::pareto cost, on points of a 10 x 10 grid with
// objectives x + y and (9 - x) + y: the front is the row y = 0.
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/pareto_late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>

#include <algorithm>
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
                {.history_length = 10, .max_iterations = 2000})
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
            "past max_iterations the search stops when mostly idle");
    }

    {
        std::mt19937 rng{11U};
        const auto result =
            grid_runner<ParetoLateAcceptanceHillClimbing>(
                {.history_length = 10, .max_iterations = 2000, .second_chance = false})
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

    return ok ? 0 : 1;
}
