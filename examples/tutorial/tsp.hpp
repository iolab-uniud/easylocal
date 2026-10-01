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
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tutorial
{

// [model] ------------------------------------------------------------------
struct Tsp
{
    std::size_t cities{};
    std::vector<double> distance; // cities x cities, row-major

    [[nodiscard]] auto d(std::size_t from, std::size_t to) const -> double
    {
        return distance[from * cities + to];
    }
};

struct Tour
{
    std::vector<std::size_t> order;

    auto operator==(const Tour&) const -> bool = default; // used by Tester checks
};

struct TwoOpt
{
    std::size_t i; // reverse the segment order[i + 1 .. j]
    std::size_t j;

    auto operator==(const TwoOpt&) const -> bool = default; // used by Tester checks
};
// [model] ------------------------------------------------------------------

// [io] ---------------------------------------------------------------------
// Optional hooks, found by ADL, that let the tools load, save and display.
[[nodiscard]] inline auto read_input(std::type_identity<Tsp>, std::istream& in) -> Tsp
{
    Tsp tsp; // "n d00 d01 ... d(n-1)(n-1)"
    if (!(in >> tsp.cities))
    {
        throw std::runtime_error{"invalid TSP header"};
    }
    tsp.distance.resize(tsp.cities * tsp.cities);
    for (auto& value : tsp.distance)
    {
        if (!(in >> value))
        {
            throw std::runtime_error{"invalid TSP distances"};
        }
    }
    return tsp;
}

[[nodiscard]] inline auto read_solution(const Tsp& tsp, std::istream& in) -> Tour
{
    Tour tour{std::vector<std::size_t>(tsp.cities)};
    for (auto& city : tour.order)
    {
        if (!(in >> city))
        {
            throw std::runtime_error{"invalid tour"};
        }
    }
    return tour;
}

inline void write_solution(const Tsp&, const Tour& tour, std::ostream& out)
{
    for (const auto city : tour.order)
    {
        out << city << ' ';
    }
    out << '\n';
}

[[nodiscard]] inline auto describe(const Tour& tour) -> std::string
{
    std::string text;
    for (const auto city : tour.order)
    {
        text += std::to_string(city) + ' ';
    }
    return text;
}

[[nodiscard]] inline auto describe(const TwoOpt& move) -> std::string
{
    return "2-opt(" + std::to_string(move.i) + ", " + std::to_string(move.j) + ")";
}
// [io] ---------------------------------------------------------------------

// [solution-manager] -------------------------------------------------------
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

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] auto random_solution(RNG& rng) const -> Tour
    {
        auto tour = initial_solution();
        std::shuffle(tour.order.begin(), tour.order.end(), rng);
        return tour;
    }

    [[nodiscard]] auto is_valid(const Tour& tour) const -> bool
    {
        return tour.order.size() == input_.cities;
    }
};
// [solution-manager] -------------------------------------------------------

// [cost-component] ---------------------------------------------------------
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
// [cost-component] ---------------------------------------------------------

// [second-component] -------------------------------------------------------
class MaxEdge
{
public:
    explicit MaxEdge(const Tsp& tsp) : tsp_{tsp} {}

    [[nodiscard]] auto evaluate(const Tour& tour) const -> double
    {
        double longest = 0.0;
        for (std::size_t k = 0; k < tour.order.size(); ++k)
        {
            longest = std::max(
                longest,
                tsp_.d(tour.order[k], tour.order[(k + 1) % tour.order.size()]));
        }
        return longest;
    }

private:
    const Tsp& tsp_;
};
// [second-component] -------------------------------------------------------

// [neighborhood] -----------------------------------------------------------
class TwoOptExplorer
    : public easylocal::neighborhood_explorer_base<TourManager, TwoOpt>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static auto name() -> std::string_view { return "2-opt"; }

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

    // [random-move]
    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] auto random_move(const Tour& tour, RNG& rng) const
        -> std::optional<TwoOpt>
    {
        const auto all = moves(tour);
        if (all.empty())
        {
            return std::nullopt;
        }
        std::uniform_int_distribution<std::size_t> pick{0, all.size() - 1};
        return all[pick(rng)];
    }
    // [random-move]

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
// [neighborhood] -----------------------------------------------------------

// [delta] ------------------------------------------------------------------
class TwoOptLengthDelta
{
public:
    explicit TwoOptLengthDelta(const Tsp& tsp) : tsp_{tsp} {}

    [[nodiscard]] auto delta_evaluate(const Tour& tour, const TwoOpt& move) const
        -> double
    {
        const auto n = tour.order.size();
        const auto a = tour.order[move.i];
        const auto b = tour.order[move.i + 1];
        const auto c = tour.order[move.j];
        const auto d = tour.order[(move.j + 1) % n];
        return tsp_.d(a, c) + tsp_.d(b, d) - tsp_.d(a, b) - tsp_.d(c, d);
    }

private:
    const Tsp& tsp_;
};
// [delta] ------------------------------------------------------------------

// [swap] -------------------------------------------------------------------
struct Swap
{
    std::size_t first;
    std::size_t second;
};

// A second neighborhood, without a delta evaluator: its moves are evaluated by
// re-evaluating TourLength on a candidate solution.
class SwapExplorer : public easylocal::neighborhood_explorer_base<TourManager, Swap>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] auto moves(const Tour& tour) const -> std::vector<Swap>
    {
        std::vector<Swap> result;
        for (std::size_t first = 0; first < tour.order.size(); ++first)
        {
            for (std::size_t second = first + 1; second < tour.order.size(); ++second)
            {
                result.push_back({first, second});
            }
        }
        return result;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] auto random_move(const Tour& tour, RNG& rng) const
        -> std::optional<Swap>
    {
        if (tour.order.size() < 2)
        {
            return std::nullopt;
        }
        std::uniform_int_distribution<std::size_t> pick{0, tour.order.size() - 1};
        const auto first = pick(rng);
        auto second = pick(rng);
        while (second == first)
        {
            second = pick(rng);
        }
        return Swap{std::min(first, second), std::max(first, second)};
    }

    [[nodiscard]] auto is_valid(const Tour& tour, const Swap& move) const -> bool
    {
        return move.first < move.second && move.second < tour.order.size();
    }

    void make_move(Tour& tour, const Swap& move) const
    {
        std::swap(tour.order[move.first], tour.order[move.second]);
    }
};
// [swap] -------------------------------------------------------------------

// [custom-runner] ----------------------------------------------------------
struct RandomDescentParameters
{
    std::size_t max_evaluations{1000};
};

class RandomDescent
{
public:
    using parameters_type = RandomDescentParameters; // for app registration

    explicit RandomDescent(RandomDescentParameters parameters)
        : parameters_{parameters}
    {
    }

    template<class Run, std::uniform_random_bit_generator RNG>
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution); // evaluates, emits run_started

        while (!run.should_stop()) // cancellation or exhausted budget
        {
            auto move = run.random_move(solution, rng);
            if (!move)
            {
                break;
            }

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            if (run.better(candidate.cost(), current.cost()))
            {
                run.commit(solution, current, std::move(candidate), *move);
            }
        }
        return run.finish(std::move(solution), current.cost()); // run_finished
    }

private:
    RandomDescentParameters parameters_;
};
// [custom-runner] ----------------------------------------------------------

// [instance] ---------------------------------------------------------------
[[nodiscard]] inline auto five_cities() -> Tsp
{
    return Tsp{
        .cities = 5,
        .distance = {
            0, 2, 9, 10, 7,
            2, 0, 6, 4, 3,
            9, 6, 0, 8, 5,
            10, 4, 8, 0, 6,
            7, 3, 5, 6, 0,
        },
    };
}
// [instance] ---------------------------------------------------------------

} // namespace tutorial
