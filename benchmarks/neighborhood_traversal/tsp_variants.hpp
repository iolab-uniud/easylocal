#pragma once

#include "generator.hpp"
#include "std_generator_support.hpp"

#include "../../examples/tsp/neighborhood_explorer.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace easylocal::benchmark::neighborhood_traversal::tsp
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

#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
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

} // namespace easylocal::benchmark::neighborhood_traversal::tsp
