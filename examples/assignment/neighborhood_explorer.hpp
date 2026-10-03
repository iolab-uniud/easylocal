#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

namespace assignment
{

class ReassignJobNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<
          AssignmentSolutionManager,
          ReassignJobMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    static constexpr std::string_view name()
    {
        return "Reassign job";
    }

    bool is_valid(const AssignmentSolution& solution, const ReassignJobMove& move) const
    {
        return move.job < solution.assignment.size()
            && move.destination < input().capacity.size()
            && solution.assignment[move.job] != move.destination;
    }

    bool first_move(const AssignmentSolution& solution, ReassignJobMove& move) const
    {
        const auto machine_count = input().capacity.size();

        if (solution.assignment.empty() || machine_count < 2)
            return false;

        move.job = 0;
        move.destination = first_destination(solution, move.job);
        return true;
    }

    bool next_move(const AssignmentSolution& solution, ReassignJobMove& move) const
    {
        const auto machine_count = input().capacity.size();
        const auto current_machine = solution.assignment[move.job];

        for (auto destination = move.destination + 1; destination < machine_count;
            ++destination)
        {
            if (destination != current_machine)
            {
                move.destination = destination;
                return true;
            }
        }

        if (move.job + 1 < solution.assignment.size())
        {
            ++move.job;
            move.destination = first_destination(solution, move.job);
            return true;
        }

        return false;
    }

    template<std::uniform_random_bit_generator RNG>
    std::optional<ReassignJobMove> random_move(
        const AssignmentSolution& solution,
        RNG& rng) const
    {
        const auto count = move_count(solution);
        if (count == 0)
            return std::nullopt;

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        return move_at(solution, draw(rng));
    }

    void make_move(AssignmentSolution& solution, const ReassignJobMove& move) const
    {
        solution.assignment[move.job] = move.destination;
    }

private:
    machine_id first_destination(
        const AssignmentSolution& solution,
        std::size_t job) const
    {
        assert(input().capacity.size() >= 2);
        assert(job < solution.assignment.size());

        return solution.assignment[job] == 0 ? machine_id{1} : machine_id{0};
    }

    std::size_t move_count(const AssignmentSolution& solution) const
    {
        const auto machine_count = input().capacity.size();
        const auto alternatives_per_job =
            machine_count > 0 ? machine_count - 1 : std::size_t{0};

        return solution.assignment.size() * alternatives_per_job;
    }

    ReassignJobMove move_at(const AssignmentSolution& solution, std::size_t ordinal) const
    {
        const auto machine_count = input().capacity.size();
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

} // namespace assignment
