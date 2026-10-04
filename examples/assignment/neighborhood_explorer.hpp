#pragma once

// The neighborhood explorer of the assignment problem: move one job to
// another machine.

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/utils/generator.hpp>

#include <cstddef>
#include <optional>
#include <random>
#include <string_view>

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

    // Every job to every machine other than its own, in job order.
    easylocal::generator<ReassignJobMove> moves(const AssignmentSolution& solution) const
    {
        for (job_id job = 0; job < solution.assignment.size(); ++job)
            for (machine_id machine = 0; machine < input().capacity.size(); ++machine)
                if (machine != solution.assignment[job])
                    co_yield ReassignJobMove{.job = job, .destination = machine};
    }

    bool is_valid(const AssignmentSolution& solution, const ReassignJobMove& move) const
    {
        return move.job < solution.assignment.size()
            && move.destination < input().capacity.size()
            && solution.assignment[move.job] != move.destination;
    }

    // One of the moves, drawn uniformly; none when there is no move.
    template<std::uniform_random_bit_generator RNG>
    std::optional<ReassignJobMove> random_move(
        const AssignmentSolution& solution,
        RNG& rng) const
    {
        const auto machine_count = input().capacity.size();
        const auto alternatives = machine_count > 0 ? machine_count - 1 : std::size_t{0};
        const auto count = solution.assignment.size() * alternatives;

        if (count == 0)
            return std::nullopt;

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        const auto ordinal = draw(rng);
        const auto job = ordinal / alternatives;
        const auto offset = ordinal % alternatives;
        const auto current = solution.assignment[job];
        const auto destination = offset < current ? offset : offset + 1;

        return ReassignJobMove{
            .job = job,
            .destination = destination,
        };
    }

    void make_move(AssignmentSolution& solution, const ReassignJobMove& move) const
    {
        solution.assignment[move.job] = move.destination;
    }
};

} // namespace assignment
