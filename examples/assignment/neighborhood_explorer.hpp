#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/service_base.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <random>
#include <utility>

namespace easylocal::mwe::assignment
{

class ReassignJobNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<
          AssignmentSolutionManager,
          ReassignJobMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;
    [[nodiscard]]
    auto is_valid(
        const AssignmentSolution& solution,
        const ReassignJobMove& move) const noexcept -> bool
    {
        const auto& instance = solution_manager_.instance();

        return move.job < solution.assignment.size() &&
               move.destination < instance.capacity.size() &&
               solution.assignment[move.job] != move.destination;
    }

    [[nodiscard]]
    auto first_move(
        const AssignmentSolution& solution,
        ReassignJobMove& move) const noexcept -> bool
    {
        assert(solution_manager_.is_valid(solution));

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
        const AssignmentSolution& solution,
        ReassignJobMove& move) const noexcept -> bool
    {
        assert(solution_manager_.is_valid(solution));
        assert(is_valid(solution, move));

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

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const AssignmentSolution& solution,
        RNG& rng) const -> std::optional<ReassignJobMove>
    {
        assert(solution_manager_.is_valid(solution));
        const auto count = move_count(solution);
        if (count == 0)
        {
            return std::nullopt;
        }

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        return move_at(solution, draw(rng));
    }


    void make_move(
        AssignmentSolution& solution,
        const ReassignJobMove& move) const noexcept
    {
        assert(is_valid(solution, move));
        solution.assignment[move.job] = move.destination;
    }

private:
    [[nodiscard]]
    auto first_destination(
        const AssignmentSolution& solution,
        const std::size_t job) const noexcept -> machine_id
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        assert(machine_count >= 2);
        assert(job < solution.assignment.size());

        return solution.assignment[job] == 0
            ? machine_id{1}
            : machine_id{0};
    }

    [[nodiscard]]
    auto move_count(const AssignmentSolution& solution) const noexcept
        -> std::size_t
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        const auto alternatives_per_job =
            machine_count > 0 ? machine_count - 1 : std::size_t{0};

        return solution.assignment.size() * alternatives_per_job;
    }

    [[nodiscard]]
    auto move_at(
        const AssignmentSolution& solution,
        const std::size_t ordinal) const noexcept -> ReassignJobMove
    {
        const auto machine_count =
            solution_manager_.instance().capacity.size();
        const auto alternatives_per_job = machine_count - 1;

        assert(alternatives_per_job != 0);
        assert(ordinal < move_count(solution));

        const auto job = ordinal / alternatives_per_job;
        const auto offset = ordinal % alternatives_per_job;
        const auto current = solution.assignment[job];
        const auto destination = offset < current ? offset : offset + 1;

        return ReassignJobMove{
            .job = job,
            .destination = destination,
        };
    }


};

} // namespace easylocal::mwe::assignment
