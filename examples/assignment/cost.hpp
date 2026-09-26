#pragma once

#include <easylocal/aggregation.hpp>
#include "cost_components.hpp"

#include <cstdint>

namespace easylocal::mwe::assignment
{

using HardCost = easylocal::aggregation::lexicographic_cost<std::int64_t, std::int64_t>;
using SoftCost = std::int64_t;
using Cost = easylocal::aggregation::hierarchical_cost<HardCost, SoftCost>;

struct AssignmentHardCostAggregator
{
    using cost_type = HardCost;

    [[nodiscard]]
    constexpr auto operator()(const CapacityValue& capacity) const -> cost_type
    {
        return easylocal::aggregation::lexicographic{}(
            capacity.total_overload,
            capacity.overloaded_machines);
    }
};

struct AssignmentCostAggregator
{
    using cost_type = Cost;

    [[nodiscard]]
    constexpr auto operator()(
        const CapacityValue& capacity,
        const SoftCost load_imbalance) const -> cost_type
    {
        return easylocal::aggregation::hierarchical{}(
            AssignmentHardCostAggregator{}(capacity),
            load_imbalance);
    }
};

} // namespace easylocal::mwe::assignment
