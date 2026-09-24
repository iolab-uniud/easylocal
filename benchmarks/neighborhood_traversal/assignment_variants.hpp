#pragma once

#include "generator.hpp"
#include "std_generator_support.hpp"

#include "../../examples/assignment/neighborhood_explorer.hpp"

#include <cassert>
#include <cstddef>

namespace easylocal::benchmark::neighborhood_traversal::assignment
{

using mwe::assignment::Move;
using mwe::assignment::Solution;
using mwe::assignment::SolutionManager;

class CoroutineNeighborhoodExplorer
{
public:
    using instance_type = typename SolutionManager::instance_type;
    using solution_type = Solution;
    using move_type = Move;

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

    void make_move(Solution& solution, const Move& move) const noexcept
    {
        solution.assignment[move.job] = move.destination;
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const -> generator<Move>
    {
        assert(solution_manager_.is_valid(solution));

        const auto machine_count =
            solution_manager_.instance().capacity.size();

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            for (std::size_t destination = 0;
                 destination < machine_count;
                 ++destination)
            {
                if (destination != solution.assignment[job])
                {
                    co_yield Move{
                        .job = job,
                        .destination = destination,
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
    using move_type = Move;

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

    void make_move(Solution& solution, const Move& move) const noexcept
    {
        solution.assignment[move.job] = move.destination;
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const -> std::generator<Move>
    {
        assert(solution_manager_.is_valid(solution));

        const auto machine_count =
            solution_manager_.instance().capacity.size();

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            for (std::size_t destination = 0;
                 destination < machine_count;
                 ++destination)
            {
                if (destination != solution.assignment[job])
                {
                    co_yield Move{
                        .job = job,
                        .destination = destination,
                    };
                }
            }
        }
    }

private:
    const SolutionManager& solution_manager_;
};
#endif

} // namespace easylocal::benchmark::neighborhood_traversal::assignment
