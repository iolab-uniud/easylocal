#pragma once

// What the contract checks share: the text of the moves and values of their
// failures, the guard that turns an exception of a user hook into a failed
// check, the moves a check visits and the solutions it starts from.

#include <easylocal/cost/text.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/fixture.hpp>
#include <easylocal/utils/detail/describe.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <exception>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace easylocal::testing::detail
{

// The text of a value in a failure message: a cost as cost::to_text writes it,
// another value from its describe hook or operator<<; "?" without one.
template<class T>
[[nodiscard]]
std::string value_text(const T& value)
{
    if constexpr (cost::text_readable<T>)
        return cost::to_text(value);
    else if constexpr (easylocal::detail::io::describable_value<T>)
        return easylocal::detail::io::describe_text(value);
    else
        return "?";
}

// "move 3 (swap 1 4)", or "move 3" for a move without a text.
template<class Move>
[[nodiscard]]
std::string move_label(const std::size_t index, const Move& move)
{
    std::string label = "move " + std::to_string(index);
    if constexpr (easylocal::detail::io::describable_value<Move>)
        label += " (" + easylocal::detail::io::describe_text(move) + ")";
    return label;
}

// Runs hook, a call of user hooks; when it throws, records a failed check
// named check_name, with context and what the exception says, and returns
// false.
template<class Hook>
bool guarded(
    check_report& report,
    const std::string_view check_name,
    const std::string& context,
    Hook&& hook)
{
    try
    {
        std::forward<Hook>(hook)();
        return true;
    }
    catch (const std::exception& error)
    {
        report.check(false, check_name, context + ": threw: " + error.what());
    }
    catch (...)
    {
        report.check(
            false,
            check_name,
            context + ": threw an exception that is not a std::exception");
    }
    return false;
}

// The enumerated moves of solution that accept keeps, at most limit of them:
// all of them in order when there are no more, otherwise a uniform sample
// (reservoir sampling), so that a check is not confined to the first moves of
// the enumeration.
template<class NHE, class Solution, class Accept>
[[nodiscard]]
std::vector<typename NHE::move_type> sample_moves(
    const NHE& neighborhood,
    const Solution& solution,
    const std::size_t limit,
    std::mt19937_64& rng,
    Accept&& accept)
{
    using move_type = typename NHE::move_type;
    std::vector<move_type> sample;
    if (limit == 0)
        return sample;
    std::size_t seen = 0;
    for (auto&& raw_move : easylocal::moves(neighborhood, solution))
    {
        move_type move{raw_move};
        if (!accept(move))
            continue;
        ++seen;
        if (sample.size() < limit)
        {
            sample.push_back(std::move(move));
            continue;
        }
        std::uniform_int_distribution<std::size_t> slot{0, seen - 1};
        if (const auto index = slot(rng); index < limit)
            sample[index] = std::move(move);
    }
    return sample;
}

// The same, with every enumerated move.
template<class NHE, class Solution>
[[nodiscard]]
std::vector<typename NHE::move_type> sample_moves(
    const NHE& neighborhood,
    const Solution& solution,
    const std::size_t limit,
    std::mt19937_64& rng)
{
    return sample_moves(neighborhood, solution, limit, rng, [](const auto&) {
        return true;
    });
}

// A random valid move of solution: drawn by random_move() when the
// neighborhood draws moves, otherwise one of its valid enumerated moves.
template<class NHE, class Solution>
[[nodiscard]]
std::optional<typename NHE::move_type> walk_move(
    const NHE& neighborhood,
    const Solution& solution,
    std::mt19937_64& rng)
{
    if constexpr (random_neighborhood_for<NHE, Solution, std::mt19937_64>)
    {
        auto move = easylocal::random_move(neighborhood, solution, rng);
        if (move && !static_cast<bool>(neighborhood.is_valid(solution, *move)))
            return std::nullopt;
        return move;
    }
    else
    {
        auto moves = sample_moves(neighborhood, solution, 1, rng, [&](const auto& move) {
            return static_cast<bool>(neighborhood.is_valid(solution, move));
        });
        if (moves.empty())
            return std::nullopt;
        return std::move(moves.front());
    }
}

// Calls visit(solution, description) on the solutions a check starts from:
// first (described as first_name), then options.random_solutions others, each drawn with
// random_solution() when the SolutionManager has one (or first otherwise),
// then walked by options.walk_length random moves. The moves of a check are so
// not those of one solution only, such as an identity tour, on which a delta
// that confuses positions and cities agrees with the full evaluation. A start
// that a hook leaves invalid, or that throws, is skipped: the other checks
// report it.
template<class SM, class NHE, class Visit>
void for_each_start(
    const SM& solution_manager,
    const NHE& neighborhood,
    const typename SM::solution_type& first,
    const std::string_view first_name,
    const check_options& options,
    std::mt19937_64& rng,
    Visit&& visit)
{
    using solution_manager_type = SM;
    using solution_type = typename SM::solution_type;

    visit(first, std::string{first_name});

    constexpr bool draws_solutions =
        has_random_solution<solution_manager_type, std::mt19937_64>;
    for (std::size_t start = 0; start < options.random_solutions; ++start)
    {
        std::optional<solution_type> solution;
        std::string description;
        std::size_t steps = 0;
        bool valid = false;
        try
        {
            if constexpr (draws_solutions)
            {
                solution.emplace(solution_manager.random_solution(rng));
                description = "random solution " + std::to_string(start + 1);
            }
            else
            {
                solution.emplace(first);
                description = first_name;
            }
            for (; steps < options.walk_length; ++steps)
            {
                auto move = walk_move(neighborhood, *solution, rng);
                if (!move)
                    break;
                neighborhood.make_move(*solution, *move);
            }
            valid = static_cast<bool>(solution_manager.is_valid(*solution));
        }
        catch (...)
        {
            // The hook that threw fails its own check.
            continue;
        }
        if (steps == 0 && !draws_solutions)
            return; // the fixture's start again
        if (!valid)
            continue;
        if (steps != 0)
            description += " after " + std::to_string(steps) + " random moves";
        visit(*solution, description);
    }
}

// The random moves of solution against its enumeration, when the
// neighborhood has both and moves compare: each drawn move is enumerated, a
// draw finds a move when the enumeration has a valid one, and two generators
// with the same seed draw the same moves (randomness drawn from elsewhere than
// the generator given repeats nothing).
template<class NHE, class Solution>
void check_random_moves_against_enumeration(
    check_report& report,
    const NHE& neighborhood,
    const Solution& solution,
    const check_options& options)
{
    using move_type = typename NHE::move_type;
    if constexpr (deterministic_neighborhood_for<NHE, Solution>
        && random_neighborhood_for<NHE, Solution, std::mt19937_64>
        && std::equality_comparable<move_type>)
    {
        std::vector<move_type> enumerated;
        if (!guarded(report, "random moves", "enumerating the moves", [&] {
                for (auto&& raw_move : easylocal::moves(neighborhood, solution))
                {
                    move_type move{raw_move};
                    if (static_cast<bool>(neighborhood.is_valid(solution, move)))
                        enumerated.push_back(std::move(move));
                }
            }))
            return;

        std::mt19937_64 first{options.seed};
        std::mt19937_64 second{options.seed};
        guarded(report, "random moves", "drawing random moves", [&] {
            for (std::size_t sample = 0; sample < options.random_samples; ++sample)
            {
                const auto move = easylocal::random_move(neighborhood, solution, first);
                const auto again = easylocal::random_move(neighborhood, solution, second);
                const auto label = "draw " + std::to_string(sample);
                report.check(
                    move.has_value() == again.has_value()
                        && (!move.has_value() || *move == *again),
                    "random move reproducibility",
                    label
                        + ": two generators with the same seed drew different moves: "
                          "does random_move draw from another source than its rng?");
                if (!move.has_value())
                {
                    report.check(
                        enumerated.empty(),
                        "random move availability",
                        label
                            + ": random_move found no move, but the neighborhood "
                              "enumerates "
                            + std::to_string(enumerated.size()) + " valid ones");
                    continue;
                }
                if (!static_cast<bool>(neighborhood.is_valid(solution, *move)))
                    continue; // reported by the random proposal check
                report.check(
                    std::ranges::find(enumerated, *move) != enumerated.end(),
                    "random move in the neighborhood",
                    move_label(sample, *move)
                        + ": random_move drew a move that moves() does not "
                          "enumerate");
            }
        });
    }
}

} // namespace easylocal::testing::detail
