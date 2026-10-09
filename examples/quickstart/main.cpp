// Quick start: a swap-move First Improvement for the symmetric TSP.
// This is the program of docs/quick-start.md; it is built and run as a test.
#include <easylocal/easylocal.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <ranges>
#include <utility>
#include <vector>

// [problem] ----------------------------------------------------------------
// Input: the instance, immutable during the search.
struct Tsp
{
    // distance[a][b] is the distance between cities a and b (symmetric).
    std::vector<std::vector<double>> distance;

    std::size_t cities() const
    {
        return distance.size();
    }
};

// Solution: the state the search modifies. order[k] is the k-th city visited;
// after the last city the tour returns to order[0].
struct Tour
{
    std::vector<std::size_t> order;
};

// Move: a local change of a Tour. It exchanges the cities visited at
// positions i and j, with i < j.
struct SwapCities
{
    std::size_t i;
    std::size_t j;
};
// [problem] ----------------------------------------------------------------

// [solution-manager] -------------------------------------------------------
// SolutionManager: how to build a solution and which solutions are valid.
class TourManager : public easylocal::solution_manager_base<Tsp, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    // The cities in index order: 0, 1, ..., n - 1.
    Tour initial_solution() const
    {
        Tour tour{std::vector<std::size_t>(input().cities())};
        std::ranges::iota(tour.order, std::size_t{0});
        return tour;
    }

    // A tour is valid when it is a permutation of 0, 1, ..., n - 1: it has
    // n positions and visits every city exactly once.
    bool is_valid(const Tour& tour) const
    {
        return std::ranges::is_permutation(
            tour.order,
            std::views::iota(std::size_t{0}, input().cities()));
    }
};
// [solution-manager] -------------------------------------------------------

// [cost] -------------------------------------------------------------------
// Cost component: one term of the objective, here the length of the tour.
class TourLength : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base; // the Input, which input() gives back

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double length = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to =
                tour.order[(k + 1) % n]; // the last city goes back to the first
            length += input().distance[from][to];
        }
        return length;
    }
};
// [cost] -------------------------------------------------------------------

// [neighborhood] -----------------------------------------------------------
// NeighborhoodExplorer: which moves exist and how they change a solution.
class SwapExplorer : public easylocal::neighborhood_explorer_base<TourManager, SwapCities>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    // Every pair of positions i < j, one move at a time: the generator hands a
    // move to the runner at each co_yield and resumes when it asks for the
    // next one, so the neighborhood is never stored in memory.
    easylocal::generator<SwapCities> moves(const Tour& tour) const
    {
        const auto n = tour.order.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                co_yield SwapCities{i, j};
    }

    bool is_valid(const Tour& tour, const SwapCities& move) const
    {
        return move.i < move.j && move.j < tour.order.size();
    }

    void make_move(Tour& tour, const SwapCities& move) const
    {
        std::swap(tour.order[move.i], tour.order[move.j]);
    }
};
// [neighborhood] -----------------------------------------------------------

int main()
{
    // [instance] -----------------------------------------------------------
    const Tsp tsp{
        .distance =
            {
                {0, 2, 9, 10, 7},
                {2, 0, 6, 4, 3},
                {9, 6, 0, 8, 5},
                {10, 4, 8, 0, 6},
                {7, 3, 5, 6, 0},
            },
    };
    // [instance] -----------------------------------------------------------

    // [runner] -------------------------------------------------------------
    // A runner is assembled from recipes: descriptions of the services that
    // are built later, by bind, when the Input is known.
    auto runner =
        // The algorithm and its parameters: the defaults run First Improvement
        // until a local optimum, with no budget on the evaluations.
        easylocal::make_runner<easylocal::runners::FirstImprovement>(
            easylocal::runners::FirstImprovementParameters{})
        // The SolutionManager with its cost: the parentheses make one recipe
        // of the manager and the cost component attached to it.
        | (easylocal::solution_manager<TourManager>()
            | easylocal::component<TourLength>())
        // The neighborhood: the moves the algorithm explores.
        | easylocal::neighborhood<SwapExplorer>();

    // bind builds TourManager, TourLength and SwapExplorer for this instance
    // and returns the bound runner; run searches from the initial tour.
    auto search = runner.bind(tsp);
    const auto result = search.run(search.initial_solution());
    // [runner] -------------------------------------------------------------

    std::cout << "length " << result.cost << " after " << result.evaluations
              << " evaluations\n";
}
