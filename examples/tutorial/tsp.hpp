#pragma once

// The running example of docs/tutorial: a symmetric TSP built up chapter by
// chapter. The tutorial's snippets are the marked regions of this file and of
// the other programs of examples/tutorial, which are compiled and run as
// tests.

#include <easylocal/config/parameters.hpp>
#include <easylocal/config/parameters_base.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/utils/input_base.hpp>
#include <easylocal/utils/limit.hpp>
#include <easylocal/utils/termination.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
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
};

// Move: exchange the cities visited at positions i and j, with i < j.
struct SwapCities
{
    std::size_t i;
    std::size_t j;
};
// [model] ------------------------------------------------------------------

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

    // [random-solution]
    // The same cities in a random order.
    template<std::uniform_random_bit_generator RNG>
    Tour random_solution(RNG& rng) const
    {
        auto tour = initial_solution();
        std::shuffle(tour.order.begin(), tour.order.end(), rng);
        return tour;
    }
    // [random-solution]

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
    // [component-text]
    // Optional, for people: the name of the component and a text that
    // explains its value on a tour, here the edges it adds up (chapter 11).
    static std::string_view name()
    {
        return "TourLength";
    }

    std::string describe(const Tour& tour) const
    {
        const auto n = tour.order.size();
        std::string text;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto edge = input().distance[tour.order[k]][tour.order[(k + 1) % n]];
            text += (k == 0 ? "" : " + ") + std::format("{}", edge);
        }
        return text;
    }
    // [component-text]
};
// [cost-component] ---------------------------------------------------------

// [second-component] -------------------------------------------------------
class MaxEdge : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double longest = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto from = tour.order[k];
            const auto to = tour.order[(k + 1) % n];
            longest = std::max(longest, input().distance[from][to]);
        }
        return longest;
    }
};
// [second-component] -------------------------------------------------------

// [domain-value] -----------------------------------------------------------
// A domain value: the edges longer than 6, as a total excess and a count.
struct LongEdges
{
    double excess{};     // the total length beyond 6 of the long edges
    std::size_t count{}; // how many edges are longer than 6

    // Needed only when LongEdges is itself the cost: tours are then compared
    // by excess first and by count between equal excesses.
    auto operator<=>(const LongEdges&) const = default;
};

class LongEdgesComponent : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    LongEdges evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        LongEdges value;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto edge = input().distance[tour.order[k]][tour.order[(k + 1) % n]];
            if (edge > 6.0)
            {
                value.excess += edge - 6.0;
                ++value.count;
            }
        }
        return value;
    }
};
// [domain-value] -----------------------------------------------------------

// [cost-parameters] --------------------------------------------------------
// The bound of chapter 2 as a parameter: how much the longest edge exceeds it.
struct ExcessParameters
{
    double bound{8.0}; // the longest edge allowed

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"bound", &ExcessParameters::bound>(
                "Longest edge allowed",
                easylocal::config::range(0.0, easylocal::unlimited)));
    }

    easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};

class Excess : public easylocal::parameters_base<ExcessParameters>
{
public:
    // The base declares parameters_type, which makes the class configurable,
    // and keeps the block, which parameters() gives back.
    using parameters_base::parameters_base;

    // Its parameters are configured under its name: cost.excess.*
    static std::string_view name()
    {
        return "excess";
    }

    double operator()(double longest) const
    {
        return std::max(0.0, longest - parameters().bound);
    }
};
// [cost-parameters] --------------------------------------------------------

// [neighborhood] -----------------------------------------------------------
class SwapExplorer : public easylocal::neighborhood_explorer_base<TourManager, SwapCities>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    // Every pair of positions i < j, one move at a time.
    easylocal::generator<SwapCities> moves(const Tour& tour) const
    {
        const auto n = tour.order.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                co_yield SwapCities{i, j};
    }

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
// [two-opt-move]
// Move: reverse the part of the tour between positions i + 1 and j.
struct TwoOpt
{
    std::size_t i;
    std::size_t j;
};
// [two-opt-move]

class TwoOptExplorer : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    // [two-opt-name]
    // The name of the neighborhood in the interactive tester (chapter 13).
    static std::string_view name()
    {
        return "2-opt";
    }
    // [two-opt-name]

    // The moves are the pairs i + 2 <= j < n, by i and then by j: with
    // j = i + 1 the segment would be one city, and with i = 0, j = n - 1 the
    // two removed edges would be the same one, so that pair is skipped.
    // A cursor enumerates them in place: first_move writes the first move into
    // `move`, next_move turns `move` into the following one, and both return
    // false when there is none.
    bool first_move(const Tour& tour, TwoOpt& move) const
    {
        move = TwoOpt{0, 1}; // just before the first move, TwoOpt{0, 2}
        return next_move(tour, move);
    }

    bool next_move(const Tour& tour, TwoOpt& move) const
    {
        const auto n = tour.order.size();
        do
        {
            if (++move.j == n) // the last j for this i: on to the next i
            {
                ++move.i;
                move.j = move.i + 2;
            }
            if (move.j >= n) // no i left
                return false;
        }
        while (move.i == 0 && move.j + 1 == n);
        return true;
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

    // The moves that moves() lists: (0, n - 1) removes the same edge twice.
    bool is_valid(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        return move.i + 2 <= move.j && move.j < n && !(move.i == 0 && move.j + 1 == n);
    }

    // The segment is the j - i cities from position i + 1: a span views it in
    // place, and reversing the view reverses those cities in the tour.
    void make_move(Tour& tour, const TwoOpt& move) const
    {
        std::ranges::reverse(std::span{tour.order}.subspan(move.i + 1, move.j - move.i));
    }
};
// [two-opt] ----------------------------------------------------------------

// [io] ---------------------------------------------------------------------
// Optional hooks, found by ADL, that read, write and describe the values
// (chapter 5).
// [read-input]
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
    // The 2-opt delta reverses a segment without its cost: symmetric only.
    for (std::size_t i = 0; i < cities; ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (tsp.distance[i][j] != tsp.distance[j][i])
                throw std::runtime_error{"the TSP distances are not symmetric"};
    return tsp;
}
// [read-input]

// [solution-io]
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
// [solution-io]

inline std::string describe(const Tour& tour)
{
    std::string text;
    for (const auto city : tour.order)
        text += std::to_string(city) + ' ';
    return text;
}

inline std::string describe(const TwoOpt& move)
{
    return "2-opt(" + std::to_string(move.i) + ", " + std::to_string(move.j) + ")";
}
// [io] ---------------------------------------------------------------------

// [delta] ------------------------------------------------------------------
class TwoOptLengthDelta : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    // The tour goes a -> b ... c -> d; after the move it goes a -> c ... b -> d,
    // the segment b ... c reversed, at the same cost with symmetric distances.
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input().distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }
};
// [delta] ------------------------------------------------------------------

// [co-located] -------------------------------------------------------------
// The tour length with its 2-opt delta in the same class: a co-located delta.
// The component is attached as usual, with component<TourLengthWithDelta>(),
// and its delta with delta<TourLengthWithDelta>().
class TourLengthWithDelta : public easylocal::input_base<Tsp>
{
public:
    using input_base::input_base;

    double evaluate(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double length = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            length += input().distance[tour.order[k]][tour.order[(k + 1) % n]];
        return length;
    }

    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        const auto& distance = input().distance;
        return distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
    }
};
// [co-located] -------------------------------------------------------------

// [equality] ---------------------------------------------------------------
// Equality of solutions and moves, for the neighborhood checks of a Session
// (chapter 14).
inline bool operator==(const Tour& a, const Tour& b)
{
    return a.order == b.order;
}

inline bool operator==(const SwapCities& a, const SwapCities& b)
{
    return a.i == b.i && a.j == b.j;
}

inline bool operator==(const TwoOpt& a, const TwoOpt& b)
{
    return a.i == b.i && a.j == b.j;
}
// [equality] ---------------------------------------------------------------

// [custom-runner] ----------------------------------------------------------
struct RandomDescentParameters
{
    easylocal::limit max_evaluations{1000}; // a count, or easylocal::unlimited

    // The schema makes the parameters configurable (chapter 9).
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "max_evaluations",
                &RandomDescentParameters::max_evaluations>(
                "Evaluation budget",
                easylocal::config::range(1, easylocal::unlimited)));
    }

    easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};

class RandomDescent : public easylocal::parameters_base<RandomDescentParameters>
{
public:
    using parameters_base::parameters_base; // configurable, built from the block

    // The result type is the one run.finish() returns: auto deduces it.
    template<class Run, std::uniform_random_bit_generator RNG>
    auto run(Run& run, Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters().max_evaluations);
        auto current = run.start(solution); // evaluates, emits run_started

        // should_stop: cancelled, the budget spent, the target reached or the
        // time up.
        while (!run.should_stop())
        {
            auto move = run.random_move(solution, rng);
            if (!move) // no move at all: a local optimum
                return run.finish(
                    std::move(solution),
                    current.cost(),
                    easylocal::termination_reason::local_optimum);

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            // A descent only ever improves: every commit is a new incumbent,
            // which commit_improvement() reports to the trace.
            if (run.better(candidate.cost(), current.cost()))
                run.commit_improvement(solution, current, std::move(candidate), *move);
        }
        return run.finish(std::move(solution), current.cost()); // run_finished
    }
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
