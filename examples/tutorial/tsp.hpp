#pragma once

// The running example of docs/tutorial: a symmetric TSP built up chapter by
// chapter. Every snippet of the tutorial is taken from this file, main.cpp or
// checks.cpp, which are compiled and run as tests.

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>

#include <algorithm>
#include <cstddef>
#include <istream>
#include <numeric>
#include <optional>
#include <ostream>
#include <random>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tutorial
{

// [model] ------------------------------------------------------------------
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

// Solution: order[k] is the k-th city visited; after the last city the tour
// returns to order[0].
struct Tour
{
    std::vector<std::size_t> order;

    bool operator==(const Tour&) const = default; // used by Tester checks
};

// Move: exchange the cities visited at positions i and j, with i < j.
struct SwapCities
{
    std::size_t i;
    std::size_t j;

    bool operator==(const SwapCities&) const = default; // used by Tester checks
};
// [model] ------------------------------------------------------------------

// [io] ---------------------------------------------------------------------
// Optional hooks, found by ADL, that let the tools load, save and display.
inline Tsp read_input(std::type_identity<Tsp>, std::istream& in)
{
    std::size_t cities = 0; // "n", then the n rows of the distance matrix
    if (!(in >> cities))
        throw std::runtime_error{"invalid TSP header"};
    Tsp tsp{.distance = std::vector(cities, std::vector<double>(cities))};
    for (auto& row : tsp.distance)
        for (auto& value : row)
            if (!(in >> value))
                throw std::runtime_error{"invalid TSP distances"};
    return tsp;
}

inline Tour read_solution(const Tsp& tsp, std::istream& in)
{
    Tour tour{std::vector<std::size_t>(tsp.cities())};
    for (auto& city : tour.order)
        if (!(in >> city))
            throw std::runtime_error{"invalid tour"};
    return tour;
}

inline void write_solution(const Tsp&, const Tour& tour, std::ostream& out)
{
    for (const auto city : tour.order)
        out << city << ' ';
    out << '\n';
}

inline std::string describe(const Tour& tour)
{
    std::string text;
    for (const auto city : tour.order)
        text += std::to_string(city) + ' ';
    return text;
}

inline std::string describe(const SwapCities& move)
{
    return "swap(" + std::to_string(move.i) + ", " + std::to_string(move.j) + ")";
}
// [io] ---------------------------------------------------------------------

// [solution-manager] -------------------------------------------------------
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

    // The same cities in a random order.
    template<std::uniform_random_bit_generator RNG>
    Tour random_solution(RNG& rng) const
    {
        auto tour = initial_solution();
        std::shuffle(tour.order.begin(), tour.order.end(), rng);
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

// [cost-component] ---------------------------------------------------------
class TourLength
{
public:
    explicit TourLength(const Tsp& input) : input_{input} {}

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double length = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to =
                tour.order[(k + 1) % n]; // the last city goes back to the first
            length += input_.distance[from][to];
        }
        return length;
    }

private:
    const Tsp& input_;
};
// [cost-component] ---------------------------------------------------------

// [second-component] -------------------------------------------------------
class MaxEdge
{
public:
    explicit MaxEdge(const Tsp& input) : input_{input} {}

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double longest = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to = tour.order[(k + 1) % n];
            longest = std::max(longest, input_.distance[from][to]);
        }
        return longest;
    }

private:
    const Tsp& input_;
};
// [second-component] -------------------------------------------------------

// [neighborhood] -----------------------------------------------------------
class SwapExplorer : public easylocal::neighborhood_explorer_base<TourManager, SwapCities>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static std::string_view name()
    {
        return "swap";
    }

    // Every pair of positions i < j, one move at a time.
    easylocal::generator<SwapCities> moves(const Tour& tour) const
    {
        const auto n = tour.order.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                co_yield SwapCities{i, j};
    }

    // [random-move]
    // Uniform: two distinct positions, each pair equally likely, put in order.
    template<std::uniform_random_bit_generator RNG>
    std::optional<SwapCities> random_move(const Tour& tour, RNG& rng) const
    {
        const auto n = tour.order.size();
        if (n < 2)
            return std::nullopt;
        std::uniform_int_distribution<std::size_t> pick{0, n - 1};
        const auto i = pick(rng);
        auto j = pick(rng);
        while (j == i)
            j = pick(rng);
        return SwapCities{std::min(i, j), std::max(i, j)};
    }
    // [random-move]

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

// [two-opt] ----------------------------------------------------------------
// Move: reverse the part of the tour between positions i + 1 and j.
struct TwoOpt
{
    std::size_t i;
    std::size_t j;

    bool operator==(const TwoOpt&) const = default; // used by Tester checks
};

class TwoOptExplorer : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static std::string_view name()
    {
        return "2-opt";
    }

    // Pairs i + 2 <= j: shorter segments would change nothing. With i = 0 and
    // j = n - 1 the two removed edges are the same one, so that pair is skipped.
    easylocal::generator<TwoOpt> moves(const Tour& tour) const
    {
        const auto n = tour.order.size();
        for (std::size_t i = 0; i + 2 < n; ++i)
            for (std::size_t j = i + 2; j < n && !(i == 0 && j + 1 == n); ++j)
                co_yield TwoOpt{i, j};
    }

    // Uniform by rejection: two positions drawn independently, ordered, and
    // drawn again while they are not a 2-opt move.
    template<std::uniform_random_bit_generator RNG>
    std::optional<TwoOpt> random_move(const Tour& tour, RNG& rng) const
    {
        const auto n = tour.order.size();
        if (n < 4)
            return std::nullopt;
        std::uniform_int_distribution<std::size_t> pick{0, n - 1};
        while (true)
        {
            auto i = pick(rng);
            auto j = pick(rng);
            if (j < i)
                std::swap(i, j);
            if (i + 2 <= j && !(i == 0 && j + 1 == n))
                return TwoOpt{i, j};
        }
    }

    bool is_valid(const Tour& tour, const TwoOpt& move) const
    {
        return move.i + 2 <= move.j && move.j < tour.order.size();
    }

    // The segment is the j - i cities from position i + 1: a span views it in
    // place, and reversing the view reverses those cities in the tour.
    void make_move(Tour& tour, const TwoOpt& move) const
    {
        std::ranges::reverse(std::span{tour.order}.subspan(move.i + 1, move.j - move.i));
    }
};
// [two-opt] ----------------------------------------------------------------

// The display hook of the 2-opt moves, for the tools (chapter 13).
inline std::string describe(const TwoOpt& move)
{
    return "2-opt(" + std::to_string(move.i) + ", " + std::to_string(move.j) + ")";
}

// [delta] ------------------------------------------------------------------
class TwoOptLengthDelta
{
public:
    explicit TwoOptLengthDelta(const Tsp& input) : input_{input} {}

    // The tour goes a -> b ... c -> d; after the move it goes a -> c ... b -> d.
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input_.distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }

private:
    const Tsp& input_;
};
// [delta] ------------------------------------------------------------------

// [custom-runner] ----------------------------------------------------------
struct RandomDescentParameters
{
    std::size_t max_evaluations{1000};
};

class RandomDescent
{
public:
    using parameters_type = RandomDescentParameters; // for app registration

    explicit RandomDescent(RandomDescentParameters parameters) : parameters_{parameters}
    {
    }

    // The result type is the one run.finish() returns: auto deduces it.
    template<class Run, std::uniform_random_bit_generator RNG>
    auto run(Run& run, Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution); // evaluates, emits run_started

        while (!run.should_stop()) // cancellation or exhausted budget
        {
            auto move = run.random_move(solution, rng);
            if (!move)
                break;

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            if (run.better(candidate.cost(), current.cost()))
                run.commit(solution, current, std::move(candidate), *move);
        }
        return run.finish(std::move(solution), current.cost()); // run_finished
    }

private:
    RandomDescentParameters parameters_;
};
// [custom-runner] ----------------------------------------------------------

// [instance] ---------------------------------------------------------------
inline Tsp five_cities()
{
    return Tsp{
        .distance =
            {
                {0, 2, 9, 10, 7},
                {2, 0, 6, 4, 3},
                {9, 6, 0, 8, 5},
                {10, 4, 8, 0, 6},
                {7, 3, 5, 6, 0},
            },
    };
}
// [instance] ---------------------------------------------------------------

} // namespace tutorial
