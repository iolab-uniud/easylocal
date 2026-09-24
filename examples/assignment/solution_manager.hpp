#pragma once

#include "cost.hpp"
#include "instance.hpp"
#include "solution.hpp"

#include <cassert>
#include <ranges>

namespace easylocal::mwe::assignment
{

class SolutionManager
{
public:
    using instance_type = Instance;
    using solution_type = Solution;

    explicit SolutionManager(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return instance_;
    }

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

private:
    const Instance& instance_;
};

} // namespace easylocal::mwe::assignment
