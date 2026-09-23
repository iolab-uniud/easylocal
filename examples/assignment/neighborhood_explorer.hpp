#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <cassert>

namespace easylocal::mwe::assignment
{

class NeighborhoodExplorer
{
public:
    explicit NeighborhoodExplorer(
        const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto is_valid(
        const Solution& solution,
        const Move& move) const noexcept -> bool
    {
        if (!solution_manager_.is_valid(solution))
        {
            return false;
        }

        const auto& instance = solution_manager_.instance();

        return move.job < solution.assignment.size() &&
               move.destination < instance.capacity.size() &&
               solution.assignment[move.job] != move.destination;
    }

    void apply(
        Solution& solution,
        const Move& move) const noexcept
    {
        assert(is_valid(solution, move));
        solution.assignment[move.job] = move.destination;
    }

private:
    const SolutionManager& solution_manager_;
};

} // namespace easylocal::mwe::assignment
