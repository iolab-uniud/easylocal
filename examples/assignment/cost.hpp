#pragma once

#include <easylocal/cost.hpp>
#include <easylocal/helpers/recipes.hpp>
#include "cost_components.hpp"

#include <cstdint>

namespace easylocal::mwe::assignment
{

using HardCost = easylocal::cost::lexicographic<std::int64_t, std::int64_t>;
using SoftCost = std::int64_t;
using Cost = easylocal::cost::hierarchical<HardCost, SoftCost>;

// The hard cost of a capacity value: total overload first, then the number of
// overloaded machines. Used in the cost expression as
// cost::apply(CapacityHardCost{}, component<CapacityCostComponent>()).
struct CapacityHardCost
{
    [[nodiscard]]
    constexpr auto operator()(const CapacityValue& capacity) const -> HardCost
    {
        return easylocal::cost::lexicographic{
            capacity.total_overload,
            capacity.overloaded_machines};
    }
};

// The cost expression of the problem: the capacity violation has strict
// priority over the load imbalance.
[[nodiscard]]
inline auto assignment_cost()
{
    return easylocal::cost::hard_soft(
        easylocal::cost::apply(
            CapacityHardCost{},
            easylocal::component<CapacityCostComponent>()),
        easylocal::component<LoadImbalanceCostComponent>());
}

} // namespace easylocal::mwe::assignment
