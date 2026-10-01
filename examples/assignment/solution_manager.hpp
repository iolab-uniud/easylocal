#pragma once

#include "cost.hpp"
#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <cassert>
#include <ranges>
#include <vector>

namespace easylocal::mwe::assignment
{

class AssignmentSolutionManager
    : public easylocal::solution_manager_base<
          AssignmentInstance,
          AssignmentSolution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    auto is_valid(const AssignmentSolution& solution) const noexcept -> bool
    {
        if (solution.assignment.size() != input_.demand.size())
        {
            return false;
        }

        return std::ranges::all_of(
            solution.assignment,
            [this](const machine_id machine) {
                return machine < input_.capacity.size();
            });
    }

    [[nodiscard]]
    auto initial_solution() const -> AssignmentSolution
    {
        assert(!input_.capacity.empty() || input_.demand.empty());

        AssignmentSolution solution{
            .assignment = std::vector<machine_id>(input_.demand.size()),
        };

        if (input_.capacity.empty())
        {
            return solution;
        }

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            solution.assignment[job] = job % input_.capacity.size();
        }

        return solution;
    }

};

} // namespace easylocal::mwe::assignment
