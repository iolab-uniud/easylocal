#pragma once

#include "cost.hpp"
#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/service_base.hpp>

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
        if (solution.assignment.size() != instance_.demand.size())
        {
            return false;
        }

        return std::ranges::all_of(
            solution.assignment,
            [this](const machine_id machine) {
                return machine < instance_.capacity.size();
            });
    }

    [[nodiscard]]
    auto initial_solution() const -> AssignmentSolution
    {
        assert(!instance_.capacity.empty() || instance_.demand.empty());

        AssignmentSolution solution{
            .assignment = std::vector<machine_id>(instance_.demand.size()),
        };

        if (instance_.capacity.empty())
        {
            return solution;
        }

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            solution.assignment[job] = job % instance_.capacity.size();
        }

        return solution;
    }

};

} // namespace easylocal::mwe::assignment
