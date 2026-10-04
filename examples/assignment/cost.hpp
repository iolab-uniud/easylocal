#pragma once

// The cost of the assignment problem: its types, and the cost expression that
// puts the capacity violation above the load imbalance.

#include "cost_components.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/helpers/recipes.hpp>

#include <cstdint>

namespace assignment
{

using HardCost = easylocal::cost::lexicographic<std::int64_t, std::int64_t>;
using SoftCost = std::int64_t;
using Cost = easylocal::cost::hierarchical<HardCost, SoftCost>;

// The hard cost of a capacity value: total overload first, then the number of
// overloaded machines. Used in the cost expression as
// cost::apply(CapacityHardCost{}, component<CapacityCostComponent>()).
struct CapacityHardCost
{
    constexpr HardCost operator()(const CapacityValue& capacity) const
    {
        return easylocal::cost::lexicographic{
            capacity.total_overload,
            capacity.overloaded_machines};
    }
};

// The cost expression of the problem: the capacity violation has strict
// priority over the load imbalance. Its type spells out the whole expression,
// so auto deduces it.
inline auto assignment_cost()
{
    return easylocal::cost::hard_soft(
        easylocal::cost::apply(
            CapacityHardCost{},
            easylocal::component<CapacityCostComponent>()),
        easylocal::component<LoadImbalanceCostComponent>());
}

} // namespace assignment
