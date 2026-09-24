#pragma once

#include "cost_components.hpp"
#include "move.hpp"

#include <cassert>
#include <cstdint>

namespace easylocal::mwe::assignment
{

struct CapacityDelta
{
    std::int64_t overloaded_machines{};
    std::int64_t total_overload{};

    auto operator==(const CapacityDelta&) const -> bool = default;
};

[[nodiscard]]
constexpr auto operator+(
    const CapacityValue value,
    const CapacityDelta delta) noexcept -> CapacityValue
{
    return CapacityValue{
        .overloaded_machines =
            value.overloaded_machines + delta.overloaded_machines,
        .total_overload = value.total_overload + delta.total_overload,
    };
}

class ReassignCapacityDeltaEvaluator
{
public:
    explicit ReassignCapacityDeltaEvaluator(const AssignmentInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto delta_evaluate(
        const AssignmentSolution& solution,
        const ReassignJobMove& move) const -> CapacityDelta
    {
        assert(solution.assignment.size() == instance_.demand.size());
        assert(move.job < solution.assignment.size());
        assert(move.destination < instance_.capacity.size());

        const auto source = solution.assignment[move.job];
        assert(source < instance_.capacity.size());
        assert(source != move.destination);

        const auto demand = instance_.demand[move.job];
        const auto source_load =
            detail::machine_load(instance_, solution, source);
        const auto destination_load =
            detail::machine_load(instance_, solution, move.destination);

        const auto source_before =
            detail::overload(source_load, instance_.capacity[source]);
        const auto destination_before = detail::overload(
            destination_load,
            instance_.capacity[move.destination]);

        const auto source_after = detail::overload(
            source_load - demand,
            instance_.capacity[source]);
        const auto destination_after = detail::overload(
            destination_load + demand,
            instance_.capacity[move.destination]);

        return CapacityDelta{
            .overloaded_machines =
                static_cast<std::int64_t>(source_after > 0) +
                static_cast<std::int64_t>(destination_after > 0) -
                static_cast<std::int64_t>(source_before > 0) -
                static_cast<std::int64_t>(destination_before > 0),
            .total_overload =
                source_after + destination_after -
                source_before - destination_before,
        };
    }

private:
    const AssignmentInstance& instance_;
};

} // namespace easylocal::mwe::assignment
