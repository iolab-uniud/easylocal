#pragma once

/// \file
/// check_neighborhood: the enumerated and random moves of the fixture Solution
/// are valid, and applying them keeps it valid.

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/fixture.hpp>

#include <concepts>
#include <cstddef>
#include <string_view>

namespace easylocal::testing
{

namespace detail
{

template<class SM, class NHE, class Solution, class Range>
void check_moves(
    check_report& report,
    std::string_view check_name,
    const SM& solution_manager,
    const NHE& neighborhood,
    const Solution& solution,
    Range&& range,
    const std::size_t limit)
{
    using move_type = typename NHE::move_type;
    std::size_t seen = 0;

    for (auto&& raw_move : range)
    {
        if (seen == limit)
        {
            break;
        }
        ++seen;

        move_type move{raw_move};
        const auto valid = static_cast<bool>(
            neighborhood.is_valid(solution, move));
        report.check(
            valid,
            check_name,
            "enumerated move does not satisfy NeighborhoodExplorer::is_valid");

        if (!valid)
        {
            continue;
        }

        auto candidate = solution;
        neighborhood.make_move(candidate, move);
        report.check(
            static_cast<bool>(solution_manager.is_valid(candidate)),
            check_name,
            "make_move produced an invalid Solution");
    }
}

} // namespace detail

/// Enumerated and sampled moves of the fixture Solution are valid, and applying
/// them keeps the Solution valid.
template<check_fixture Fixture, class NHE>
[[nodiscard]] check_report check_neighborhood(
    const Fixture& fixture,
    const NHE& neighborhood)
{
    using solution_manager_type = typename Fixture::solution_manager_type;
    using solution_type = typename Fixture::solution_type;

    static_assert(
        neighborhood_explorer_for<NHE, solution_manager_type>,
        "the NeighborhoodExplorer does not satisfy the EasyLocal NeighborhoodExplorer "
        "core contract for the fixture's SolutionManager");

    const auto& solution_manager = fixture.solution_manager();
    const auto& solution = fixture.solution();

    check_report report{"NeighborhoodExplorer"};

    if (!detail::check_fixture_solution(report, fixture))
    {
        return report;
    }

    const auto max_moves = fixture.options().max_enumerated_moves;

    if constexpr (cursor_neighborhood_for<NHE, solution_type>)
    {
        detail::check_moves(
            report,
            "cursor traversal",
            solution_manager,
            neighborhood,
            solution,
            easylocal::cursor_moves(neighborhood, solution),
            max_moves);
    }

    if constexpr (native_moves_neighborhood_for<NHE, solution_type>)
    {
        detail::check_moves(
            report,
            "native moves traversal",
            solution_manager,
            neighborhood,
            solution,
            neighborhood.moves(solution),
            max_moves);
    }

    if constexpr (random_neighborhood_for<NHE, solution_type, deterministic_rng>)
    {
        deterministic_rng rng;
        for (std::size_t sample = 0; sample < fixture.options().random_samples; ++sample)
        {
            auto move = easylocal::random_move(neighborhood, solution, rng);
            if (!move)
            {
                continue;
            }

            const auto valid = static_cast<bool>(
                neighborhood.is_valid(solution, *move));
            report.check(
                valid,
                "random proposal",
                "random_move produced a move that does not satisfy is_valid");

            if (!valid)
            {
                continue;
            }

            auto candidate = solution;
            neighborhood.make_move(candidate, *move);
            report.check(
                static_cast<bool>(solution_manager.is_valid(candidate)),
                "random proposal",
                "random move produced an invalid Solution");
        }
    }

    return report;
}

/// The same, with the NeighborhoodExplorer built on the fixture's SolutionManager.
template<class NHE, check_fixture Fixture>
[[nodiscard]] check_report check_neighborhood(const Fixture& fixture)
{
    return check_neighborhood(
        fixture,
        detail::make_neighborhood<NHE>(fixture.solution_manager()));
}

} // namespace easylocal::testing
