#pragma once

#include "cost.hpp"
#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/service_base.hpp>

#include <cassert>
#include <ranges>

namespace easylocal::mwe::assignment
{

class SolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    auto is_valid(const Solution& solution) const noexcept -> bool
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
    constexpr auto aggregate(const CapacityValue& capacity) const -> Cost
    {
        return AssignmentCostAggregator{}(capacity);
    }
};

} // namespace easylocal::mwe::assignment
