#pragma once

#include "cursor_view.hpp"
#include "generator.hpp"
#include "std_generator_support.hpp"

#include "../../examples/tsp/move.hpp"
#include "../../examples/tsp/solution_manager.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace easylocal::spike::neighborhood_authoring::tsp
{

using mwe::tsp::Solution;
using mwe::tsp::SolutionManager;
using mwe::tsp::TwoOptMove;

[[nodiscard]]
constexpr auto valid_edge_pair(
    const std::size_t city_count,
    const std::size_t first_edge,
    const std::size_t second_edge) noexcept -> bool
{
    return first_edge < second_edge &&
           second_edge < city_count &&
           second_edge != first_edge + 1 &&
           !(first_edge == 0 && second_edge + 1 == city_count);
}

class CoroutineNeighborhoodExplorer
{
public:
    using instance_type = typename SolutionManager::instance_type;
    using solution_type = Solution;
    using move_type = TwoOptMove;

    explicit CoroutineNeighborhoodExplorer(
        const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return solution_manager_.instance();
    }

    void make_move(Solution& solution, const TwoOptMove& move) const noexcept
    {
        const auto first = static_cast<std::ptrdiff_t>(move.first_edge + 1);
        const auto last = static_cast<std::ptrdiff_t>(move.second_edge + 1);
        std::reverse(
            solution.tour.begin() + first,
            solution.tour.begin() + last);
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const -> generator<TwoOptMove>
    {
        assert(solution_manager_.is_valid(solution));
        const auto city_count = solution.tour.size();

        for (std::size_t first_edge = 0;
             first_edge < city_count;
             ++first_edge)
        {
            for (std::size_t second_edge = first_edge + 1;
                 second_edge < city_count;
                 ++second_edge)
            {
                if (valid_edge_pair(
                        city_count,
                        first_edge,
                        second_edge))
                {
                    co_yield TwoOptMove{
                        .first_edge = first_edge,
                        .second_edge = second_edge,
                    };
                }
            }
        }
    }

private:
    const SolutionManager& solution_manager_;
};

#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
class StdCoroutineNeighborhoodExplorer
{
public:
    using instance_type = typename SolutionManager::instance_type;
    using solution_type = Solution;
    using move_type = TwoOptMove;

    explicit StdCoroutineNeighborhoodExplorer(
        const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return solution_manager_.instance();
    }

    void make_move(Solution& solution, const TwoOptMove& move) const noexcept
    {
        const auto first = static_cast<std::ptrdiff_t>(move.first_edge + 1);
        const auto last = static_cast<std::ptrdiff_t>(move.second_edge + 1);
        std::reverse(
            solution.tour.begin() + first,
            solution.tour.begin() + last);
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const -> std::generator<TwoOptMove>
    {
        assert(solution_manager_.is_valid(solution));
        const auto city_count = solution.tour.size();

        for (std::size_t first_edge = 0;
             first_edge < city_count;
             ++first_edge)
        {
            for (std::size_t second_edge = first_edge + 1;
                 second_edge < city_count;
                 ++second_edge)
            {
                if (valid_edge_pair(
                        city_count,
                        first_edge,
                        second_edge))
                {
                    co_yield TwoOptMove{
                        .first_edge = first_edge,
                        .second_edge = second_edge,
                    };
                }
            }
        }
    }

private:
    const SolutionManager& solution_manager_;
};
#endif

class CursorNeighborhoodExplorer
{
public:
    using instance_type = typename SolutionManager::instance_type;
    using solution_type = Solution;
    using move_type = TwoOptMove;

    explicit CursorNeighborhoodExplorer(
        const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return solution_manager_.instance();
    }

    void make_move(Solution& solution, const TwoOptMove& move) const noexcept
    {
        const auto first = static_cast<std::ptrdiff_t>(move.first_edge + 1);
        const auto last = static_cast<std::ptrdiff_t>(move.second_edge + 1);
        std::reverse(
            solution.tour.begin() + first,
            solution.tour.begin() + last);
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const noexcept
    {
        assert(solution_manager_.is_valid(solution));
        return cursor_moves(*this, solution);
    }

    [[nodiscard]]
    auto first_move(
        const Solution& solution,
        TwoOptMove& move) const noexcept -> bool
    {
        return find_from(solution.tour.size(), 0, 1, move);
    }

    [[nodiscard]]
    auto next_move(
        const Solution& solution,
        TwoOptMove& move) const noexcept -> bool
    {
        return find_from(
            solution.tour.size(),
            move.first_edge,
            move.second_edge + 1,
            move);
    }

private:
    [[nodiscard]]
    static auto find_from(
        const std::size_t city_count,
        const std::size_t initial_first_edge,
        const std::size_t initial_second_edge,
        TwoOptMove& move) noexcept -> bool
    {
        for (auto first_edge = initial_first_edge;
             first_edge < city_count;
             ++first_edge)
        {
            const auto second_begin = first_edge == initial_first_edge
                ? initial_second_edge
                : first_edge + 1;

            for (auto second_edge = second_begin;
                 second_edge < city_count;
                 ++second_edge)
            {
                if (valid_edge_pair(
                        city_count,
                        first_edge,
                        second_edge))
                {
                    move = TwoOptMove{
                        .first_edge = first_edge,
                        .second_edge = second_edge,
                    };
                    return true;
                }
            }
        }

        return false;
    }

    const SolutionManager& solution_manager_;
};

} // namespace easylocal::spike::neighborhood_authoring::tsp
