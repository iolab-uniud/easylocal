#pragma once

#include <easylocal/aggregation.hpp>
#include "cost_components.hpp"

#include <cstdint>

namespace easylocal::mwe::assignment
{

using Cost = easylocal::aggregation::lexicographic_cost<std::int64_t, std::int64_t>;

struct AssignmentCostAggregator
{
    using cost_type = Cost;

    [[nodiscard]]
    constexpr auto operator()(const CapacityValue& capacity) const -> cost_type
    {
        return easylocal::aggregation::lexicographic{}(
            capacity.total_overload,
            capacity.overloaded_machines);
    }
};

} // namespace easylocal::mwe::assignment
