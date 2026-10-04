#pragma once

// The solution manager of the assignment problem: the initial solution and
// the validity check.

#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/helpers/solution_manager.hpp>

#include <algorithm>
#include <cassert>
#include <vector>

namespace assignment
{

class AssignmentSolutionManager
    : public easylocal::solution_manager_base<AssignmentInstance, AssignmentSolution>
{
public:
    using solution_manager_base::solution_manager_base;

    bool is_valid(const AssignmentSolution& solution) const
    {
        if (solution.assignment.size() != input().demand.size())
            return false;

        return std::ranges::all_of(solution.assignment, [this](machine_id machine) {
            return machine < input().capacity.size();
        });
    }

    AssignmentSolution initial_solution() const
    {
        assert(!input().capacity.empty() || input().demand.empty());

        AssignmentSolution solution{
            .assignment = std::vector<machine_id>(input().demand.size()),
        };

        if (input().capacity.empty())
            return solution;

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
            solution.assignment[job] = job % input().capacity.size();

        return solution;
    }
};

} // namespace assignment
