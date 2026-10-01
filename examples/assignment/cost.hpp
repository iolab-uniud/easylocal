#pragma once

#include <easylocal/core/aggregation.hpp>
#include "cost_components.hpp"

#include <cstdint>

namespace easylocal::mwe::assignment
{

using HardCost = easylocal::aggregation::lexicographic_cost<std::int64_t, std::int64_t>;
using SoftCost = std::int64_t;
using Cost = easylocal::aggregation::hierarchical_cost<HardCost, SoftCost>;

struct AssignmentCostAggregator
{
    [[nodiscard]]
    constexpr auto hard(const CapacityValue& capacity) const -> HardCost
    {
        return easylocal::aggregation::lexicographic{}(
            capacity.total_overload,
            capacity.overloaded_machines);
    }

    [[nodiscard]]
    constexpr auto operator()(
        const CapacityValue& capacity,
        const SoftCost load_imbalance) const -> Cost
    {
        return easylocal::aggregation::hierarchical{}(
            hard(capacity),
            load_imbalance);
    }
};

} // namespace easylocal::mwe::assignment
