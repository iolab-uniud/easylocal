#pragma once

#include <easylocal/cost.hpp>
#include "cost_components.hpp"

#include <cstdint>

namespace easylocal::mwe::assignment
{

using HardCost = easylocal::cost::lexicographic<std::int64_t, std::int64_t>;
using SoftCost = std::int64_t;
using Cost = easylocal::cost::hierarchical<HardCost, SoftCost>;

struct AssignmentCostAggregator
{
    [[nodiscard]]
    constexpr auto hard(const CapacityValue& capacity) const -> HardCost
    {
        return easylocal::cost::lexicographic{
            capacity.total_overload,
            capacity.overloaded_machines};
    }

    [[nodiscard]]
    constexpr auto operator()(
        const CapacityValue& capacity,
        const SoftCost load_imbalance) const -> Cost
    {
        return easylocal::cost::hierarchical{
            hard(capacity),
            load_imbalance};
    }
};

} // namespace easylocal::mwe::assignment
