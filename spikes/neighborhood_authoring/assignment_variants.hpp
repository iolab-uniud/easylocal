#pragma once

#include "cursor_view.hpp"
#include "generator.hpp"
#include "std_generator_support.hpp"

#include "../../examples/assignment/move.hpp"
#include "../../examples/assignment/solution_manager.hpp"

#include <cassert>
#include <cstddef>

namespace easylocal::spike::neighborhood_authoring::assignment
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

#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
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

class CursorNeighborhoodExplorer
{
public:
    using instance_type = typename SolutionManager::instance_type;
    using solution_type = Solution;
    using move_type = Move;

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

    void make_move(Solution& solution, const Move& move) const noexcept
    {
        solution.assignment[move.job] = move.destination;
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
        Move& move) const noexcept -> bool
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();

        if (solution.assignment.empty() || machine_count < 2)
        {
            return false;
        }

        move.job = 0;
        move.destination = first_destination(solution, move.job);
        return true;
    }

    [[nodiscard]]
    auto next_move(
        const Solution& solution,
        Move& move) const noexcept -> bool
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        const auto current_machine = solution.assignment[move.job];

        for (auto destination = move.destination + 1;
             destination < machine_count;
             ++destination)
        {
            if (destination != current_machine)
            {
                move.destination = destination;
                return true;
            }
        }

        for (auto job = move.job + 1;
             job < solution.assignment.size();
             ++job)
        {
            move.job = job;
            move.destination = first_destination(solution, job);
            return true;
        }

        return false;
    }

private:
    [[nodiscard]]
    auto first_destination(
        const Solution& solution,
        const std::size_t job) const noexcept -> std::size_t
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        assert(machine_count >= 2);
        assert(job < solution.assignment.size());

        return solution.assignment[job] == 0 ? std::size_t{1} : std::size_t{0};
    }

    const SolutionManager& solution_manager_;
};

} // namespace easylocal::spike::neighborhood_authoring::assignment
