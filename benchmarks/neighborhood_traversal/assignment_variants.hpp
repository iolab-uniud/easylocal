#pragma once

#include "generator.hpp"
#include "std_generator_support.hpp"

#include "../../examples/assignment/neighborhood_explorer.hpp"

#include <cassert>
#include <cstddef>

namespace easylocal::benchmark::neighborhood_traversal::assignment
{

using mwe::assignment::ReassignJobMove;
using mwe::assignment::AssignmentSolution;
using mwe::assignment::AssignmentSolutionManager;

class CoroutineNeighborhoodExplorer
{
public:
    using instance_type = typename AssignmentSolutionManager::instance_type;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    explicit CoroutineNeighborhoodExplorer(
        const AssignmentSolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto is_valid(
        const AssignmentSolution& solution,
        const ReassignJobMove& move) const noexcept -> bool
    {
        return move.job < solution.assignment.size() &&
               move.destination < solution_manager_.instance().capacity.size() &&
               solution.assignment[move.job] != move.destination;
    }

    void make_move(AssignmentSolution& solution, const ReassignJobMove& move) const noexcept
    {
        solution.assignment[move.job] = move.destination;
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution& solution) const -> generator<ReassignJobMove>
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
                    co_yield ReassignJobMove{
                        .job = job,
                        .destination = destination,
                    };
                }
            }
        }
    }

private:
    const AssignmentSolutionManager& solution_manager_;
};

#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
class StdCoroutineNeighborhoodExplorer
{
public:
    using instance_type = typename AssignmentSolutionManager::instance_type;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    explicit StdCoroutineNeighborhoodExplorer(
        const AssignmentSolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto is_valid(
        const AssignmentSolution& solution,
        const ReassignJobMove& move) const noexcept -> bool
    {
        return move.job < solution.assignment.size() &&
               move.destination < solution_manager_.instance().capacity.size() &&
               solution.assignment[move.job] != move.destination;
    }

    void make_move(AssignmentSolution& solution, const ReassignJobMove& move) const noexcept
    {
        solution.assignment[move.job] = move.destination;
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution& solution) const -> std::generator<ReassignJobMove>
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
                    co_yield ReassignJobMove{
                        .job = job,
                        .destination = destination,
                    };
                }
            }
        }
    }

private:
    const AssignmentSolutionManager& solution_manager_;
};
#endif

} // namespace easylocal::benchmark::neighborhood_traversal::assignment
