#pragma once

#include <easylocal/cursor_moves.hpp>
#include <easylocal/neighborhood_concepts.hpp>
#include <easylocal/testing/check.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

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

template<class Test>
[[nodiscard]] auto check_neighborhood() -> check_report
{
    using neighborhood_type = typename Test::neighborhood;
    using solution_manager_type = detail::test_solution_manager_t<Test>;
    using instance_type = typename solution_manager_type::instance_type;
    using solution_type = typename solution_manager_type::solution_type;

    static_assert(
        neighborhood_explorer_for<neighborhood_type, solution_manager_type>,
        "Test::neighborhood does not satisfy the EasyLocal NeighborhoodExplorer core contract");

    auto instance = Test::instance();
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(instance)>,
        instance_type>);

    auto solution_manager = detail::make_solution_manager<
        Test, instance_type, solution_manager_type>(instance);
    auto neighborhood = detail::make_neighborhood<
        Test, solution_manager_type, neighborhood_type>(solution_manager);
    auto solution = Test::solution(instance);
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(solution)>,
        solution_type>);

    check_report report{"NeighborhoodExplorer"};

    const auto valid_solution = static_cast<bool>(
        solution_manager.is_valid(solution));
    report.check(
        valid_solution,
        "fixture solution",
        "Test::solution(instance) must return a valid Solution");

    if (!valid_solution)
    {
        return report;
    }

    constexpr auto max_moves = detail::max_enumerated_moves_v<Test>;

    if constexpr (cursor_neighborhood_for<neighborhood_type, solution_type>)
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

    if constexpr (native_moves_neighborhood_for<neighborhood_type, solution_type>)
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

    if constexpr (random_neighborhood_for<
                      neighborhood_type,
                      solution_type,
                      deterministic_rng>)
    {
        deterministic_rng rng;
        for (std::size_t sample = 0;
             sample < detail::random_samples_v<Test>;
             ++sample)
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

} // namespace easylocal::testing
