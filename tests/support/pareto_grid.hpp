#pragma once

// A problem with a cost::pareto cost, used by the tests of the front: points of
// a 10 x 10 grid, a step in one of four directions as the move, and the
// objectives x + y and (9 - x) + y, so the front is the row y = 0. The Input is
// read from any text (it holds nothing) and a point is written as "x y", for
// the tools that read and write them.
#include <easylocal/cost.hpp>
#include <easylocal/helpers/recipes.hpp>

#include <array>
#include <cstddef>
#include <istream>
#include <optional>
#include <ostream>
#include <random>

namespace pareto_grid
{

struct Grid
{
    static constexpr int side = 10;

    friend std::istream& operator>>(std::istream& in, Grid&)
    {
        return in;
    }
};

struct Point
{
    int x{};
    int y{};

    friend bool operator==(const Point&, const Point&) = default;

    friend std::ostream& operator<<(std::ostream& out, const Point& point)
    {
        return out << point.x << ' ' << point.y << '\n';
    }
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

    const Grid& input() const noexcept
    {
        return grid_;
    }

    template<std::uniform_random_bit_generator RNG>
    static Point random_solution(RNG& rng)
    {
        std::uniform_int_distribution<int> coordinate{0, Grid::side - 1};
        const auto x = coordinate(rng);
        return Point{x, coordinate(rng)};
    }

    static bool is_valid(const Point& point) noexcept
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

    const Grid& input() const noexcept
    {
        return grid_;
    }

    template<std::uniform_random_bit_generator RNG>
    static std::optional<Step> random_move(const Point& point, RNG& rng)
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

    static bool is_valid(const Point& point, const Step& step) noexcept
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
    static int evaluate(const Point& point) noexcept
    {
        return point.x + point.y;
    }
};

struct Right
{
    static int evaluate(const Point& point) noexcept
    {
        return (Grid::side - 1 - point.x) + point.y;
    }
};

// The SolutionManager of the grid with its two objectives.
inline auto grid_solution_manager()
{
    return easylocal::solution_manager<PointManager>()
        | easylocal::cost::objectives(
            easylocal::component<Left>(),
            easylocal::component<Right>());
}

} // namespace pareto_grid
