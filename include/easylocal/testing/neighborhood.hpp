#pragma once

/// \file
/// check_neighborhood: the enumerated and random moves of the fixture Solution
/// and of random solutions are valid, applying them keeps the Solution valid
/// and changes it, and the random moves are moves of the enumeration, drawn
/// from the generator given.

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/detail/support.hpp>
#include <easylocal/testing/fixture.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace easylocal::testing
{

namespace detail
{

// What the move checks count across the solutions they start from: the moves
// applied, and those that left the Solution unchanged.
struct applied_moves
{
    std::size_t applied{};
    std::size_t unchanged{};
};

// Applies move, valid, to a copy of solution: the result is valid, and
// counted as changed or not when solutions compare.
template<class SM, class NHE, class Solution, class Label>
void check_application(
    check_report& report,
    const std::string_view check_name,
    const Label& label,
    const SM& solution_manager,
    const NHE& neighborhood,
    const Solution& solution,
    const typename NHE::move_type& move,
    applied_moves& counts)
{
    auto candidate = solution;
    neighborhood.make_move(candidate, move);
    report.check(
        static_cast<bool>(solution_manager.is_valid(candidate)),
        check_name,
        [&] { return label() + ": make_move produced an invalid Solution"; });
    if constexpr (has_solution_equality<SM>)
    {
        ++counts.applied;
        counts.unchanged += solutions_equal(solution_manager, candidate, solution);
    }
}

template<class SM, class NHE, class Solution, class Range>
void check_moves(
    check_report& report,
    std::string_view check_name,
    const SM& solution_manager,
    const NHE& neighborhood,
    const Solution& solution,
    const std::string& from,
    Range&& range,
    applied_moves& counts)
{
    std::size_t index = 0;
    for (const auto& move : range)
    {
        const auto label = [&, index] {
            return move_label(index, move) + " from " + from;
        };
        ++index;
        guarded(report, check_name, label, [&] {
            const auto valid = static_cast<bool>(neighborhood.is_valid(solution, move));
            report.check(valid, check_name, [&] {
                return label() + ": an enumerated move does not satisfy is_valid";
            });
            if (valid)
                check_application(
                    report,
                    check_name,
                    label,
                    solution_manager,
                    neighborhood,
                    solution,
                    move,
                    counts);
        });
    }
}

} // namespace detail

/// Enumerated and sampled moves of the fixture Solution and of random
/// solutions are valid, and applying them keeps the Solution valid.
///
/// With solutions that compare (`==`, or the SolutionManager's `equal`), the
/// moves must not all leave the Solution unchanged, as a make_move taking the
/// Solution by value does. With moves that compare and a neighborhood that
/// both enumerates and draws, the random moves must be enumerated ones, found
/// whenever a valid move exists, and repeated by a generator with the same
/// seed. An exception of a hook is a failure. Requires a NeighborhoodExplorer
/// built on the fixture's SolutionManager.
template<check_fixture Fixture, class NHE>
[[nodiscard]] check_report check_neighborhood(
    const Fixture& fixture,
    const NHE& neighborhood)
{
    using solution_manager_type = typename Fixture::solution_manager_type;
    using solution_type = typename Fixture::solution_type;
    using move_type = typename NHE::move_type;

    static_assert(
        neighborhood_explorer_for<NHE, solution_manager_type>,
        "the NeighborhoodExplorer does not satisfy the EasyLocal NeighborhoodExplorer "
        "core contract for the fixture's SolutionManager");

    const auto& solution_manager = fixture.solution_manager();
    const auto& options = fixture.options();

    check_report report{"NeighborhoodExplorer"};

    if (!detail::check_fixture_solution(report, fixture))
    {
        return report;
    }

    detail::applied_moves counts;
    std::mt19937_64 rng{options.seed};
    detail::for_each_start(
        solution_manager,
        neighborhood,
        fixture.solution(),
        "the fixture Solution",
        options,
        rng,
        [&](const solution_type& solution, const std::string& from) {
            const auto traversal = [&](std::string_view check_name,
                                       const auto& make_range) {
                std::vector<move_type> moves;
                if (!detail::guarded(report, check_name, "enumerating from " + from, [&] {
                        std::size_t seen = 0;
                        for (auto&& raw_move : make_range())
                        {
                            if (seen++ == options.max_enumerated_moves)
                                break;
                            moves.emplace_back(raw_move);
                        }
                    }))
                    return;
                detail::check_moves(
                    report,
                    check_name,
                    solution_manager,
                    neighborhood,
                    solution,
                    from,
                    moves,
                    counts);
            };

            if constexpr (cursor_neighborhood_for<NHE, solution_type>)
                traversal("cursor traversal", [&] {
                    return easylocal::cursor_moves(neighborhood, solution);
                });
            if constexpr (native_moves_neighborhood_for<NHE, solution_type>)
                traversal("native moves traversal", [&] {
                    return neighborhood.moves(solution);
                });

            if constexpr (random_neighborhood_for<NHE, solution_type, std::mt19937_64>)
            {
                for (std::size_t sample = 0; sample < options.random_samples; ++sample)
                {
                    std::optional<move_type> move;
                    const auto drawn = "draw " + std::to_string(sample) + " from " + from;
                    if (!detail::guarded(report, "random proposal", drawn, [&] {
                            move = easylocal::random_move(neighborhood, solution, rng);
                        }))
                        break;
                    if (!move)
                        continue;
                    const auto label = [&] {
                        return detail::move_label(sample, *move) + " drawn from " + from;
                    };
                    detail::guarded(report, "random proposal", label, [&] {
                        const auto valid =
                            static_cast<bool>(neighborhood.is_valid(solution, *move));
                        report.check(valid, "random proposal", [&] {
                            return label()
                                + ": random_move produced a move that does not "
                                  "satisfy is_valid";
                        });
                        if (valid)
                            detail::check_application(
                                report,
                                "random proposal",
                                label,
                                solution_manager,
                                neighborhood,
                                solution,
                                *move,
                                counts);
                    });
                }
            }
        });

    if constexpr (has_solution_equality<solution_manager_type>)
    {
        report.check(
            counts.applied == 0 || counts.unchanged < counts.applied,
            "null moves",
            "every move applied (" + std::to_string(counts.applied)
                + ") left the Solution unchanged: does make_move take the Solution "
                  "by value instead of by reference?");
    }

    detail::check_random_moves_against_enumeration(
        report,
        neighborhood,
        fixture.solution(),
        options);

    return report;
}

/// The same, with the NeighborhoodExplorer built on the fixture's
/// SolutionManager.
template<class NHE, check_fixture Fixture>
[[nodiscard]] check_report check_neighborhood(const Fixture& fixture)
{
    return check_neighborhood(
        fixture,
        detail::make_neighborhood<NHE>(fixture.solution_manager()));
}

} // namespace easylocal::testing
