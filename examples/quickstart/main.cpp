// Quick start: a 2-opt First Improvement for the symmetric TSP.
// This is the program of docs/quick-start.md; it is built and run as a test.
#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <utility>
#include <vector>

// 1. The problem: Input, Solution and Move are plain values.
struct Tsp
{
    std::size_t cities{};
    std::vector<double> distance{}; // cities x cities, row-major

    [[nodiscard]] auto d(std::size_t from, std::size_t to) const -> double
    {
        return distance[from * cities + to];
    }
};

struct Tour
{
    std::vector<std::size_t> order;
};

struct TwoOpt
{
    std::size_t i; // reverse the segment order[i + 1 .. j]
    std::size_t j;
};

// 2. SolutionManager: what a valid solution is and how to build one.
class TourManager : public easylocal::solution_manager_base<Tsp, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] auto initial_solution() const -> Tour
    {
        Tour tour{std::vector<std::size_t>(input_.cities)};
        std::iota(tour.order.begin(), tour.order.end(), std::size_t{0});
        return tour;
    }

    [[nodiscard]] auto is_valid(const Tour& tour) const -> bool
    {
        return tour.order.size() == input_.cities;
    }
};

// 3. A cost component: one term of the objective.
class TourLength
{
public:
    explicit TourLength(const Tsp& tsp) : tsp_{tsp} {}

    [[nodiscard]] auto evaluate(const Tour& tour) const -> double
    {
        double length = 0.0;
        for (std::size_t k = 0; k < tour.order.size(); ++k)
        {
            length += tsp_.d(tour.order[k], tour.order[(k + 1) % tour.order.size()]);
        }
        return length;
    }

private:
    const Tsp& tsp_;
};

// 4. NeighborhoodExplorer: which moves exist and how they change a solution.
class TwoOptExplorer
    : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] auto moves(const Tour& tour) const -> std::vector<TwoOpt>
    {
        std::vector<TwoOpt> result;
        const auto n = tour.order.size();
        for (std::size_t i = 0; i + 2 < n; ++i)
        {
            for (std::size_t j = i + 2; j < n && !(i == 0 && j + 1 == n); ++j)
            {
                result.push_back({i, j});
            }
        }
        return result;
    }

    [[nodiscard]] auto is_valid(const Tour& tour, const TwoOpt& move) const -> bool
    {
        return move.i + 2 <= move.j && move.j < tour.order.size();
    }

    void make_move(Tour& tour, const TwoOpt& move) const
    {
        std::reverse(
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.i + 1),
            tour.order.begin() + static_cast<std::ptrdiff_t>(move.j + 1));
    }
};

int main()
{
    const Tsp tsp{
        .cities = 5,
        .distance = {
            0, 2, 9, 10, 7,
            2, 0, 6, 4, 3,
            9, 6, 0, 8, 5,
            10, 4, 8, 0, 6,
            7, 3, 5, 6, 0,
        },
    };

    // 5. Compose a runner: algorithm | SolutionManager recipe | neighborhood.
    auto runner =
        easylocal::make_runner<easylocal::runners::FirstImprovement>(
            easylocal::runners::FirstImprovementParameters{})
        | (easylocal::solution_manager<TourManager>()
           | easylocal::component<TourLength>())
        | easylocal::neighborhood<TwoOptExplorer>();

    // 6. Bind it to an Input and run it from a solution.
    auto bound = runner.bind(tsp);
    const auto result = bound.run(bound.initial_solution());

    std::cout << "length " << result.cost << " after " << result.evaluations
              << " evaluations\n";
}
