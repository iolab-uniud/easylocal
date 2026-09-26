#pragma once

#include "instance.hpp"
#include "solution.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace easylocal::mwe::assignment
{

// Typed component values are useful when the type itself carries domain meaning.
struct CapacityValue
{
    std::int64_t overloaded_machines{};
    std::int64_t total_overload{};

    auto operator==(const CapacityValue&) const -> bool = default;
};

namespace detail
{

[[nodiscard]]
inline auto machine_load(
    const AssignmentInstance& instance,
    const AssignmentSolution& solution,
    const machine_id machine) -> quantity_type
{
    assert(solution.assignment.size() == instance.demand.size());

    quantity_type load = 0;

    for (std::size_t job = 0; job < solution.assignment.size(); ++job)
    {
        if (solution.assignment[job] == machine)
        {
            load += instance.demand[job];
        }
    }

    return load;
}

[[nodiscard]]
inline auto overload(
    const quantity_type load,
    const quantity_type capacity) noexcept -> quantity_type
{
    return std::max(quantity_type{0}, load - capacity);
}

} // namespace detail

class CapacityCostComponent
{
public:
    explicit CapacityCostComponent(const AssignmentInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto evaluate(const AssignmentSolution& solution) const -> CapacityValue
    {
        assert(solution.assignment.size() == instance_.demand.size());

        std::vector<quantity_type> load(
            instance_.capacity.size(),
            quantity_type{0});

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            assert(solution.assignment[job] < instance_.capacity.size());
            load[solution.assignment[job]] += instance_.demand[job];
        }

        CapacityValue value{};

        for (std::size_t machine = 0; machine < load.size(); ++machine)
        {
            const auto machine_overload =
                detail::overload(load[machine], instance_.capacity[machine]);

            value.overloaded_machines += machine_overload > 0 ? 1 : 0;
            value.total_overload += machine_overload;
        }

        return value;
    }

private:
    const AssignmentInstance& instance_;
};

class LoadImbalanceCostComponent
{
public:
    explicit LoadImbalanceCostComponent(const AssignmentInstance& instance) noexcept
        : instance_{instance}
    {
    }

    // A component value does not need a wrapper: plain arithmetic types are
    // equally valid when a distinct semantic type would add no useful signal.
    [[nodiscard]]
    auto evaluate(const AssignmentSolution& solution) const -> std::int64_t
    {
        assert(solution.assignment.size() == instance_.demand.size());

        if (instance_.capacity.empty())
        {
            return 0;
        }

        std::vector<quantity_type> load(
            instance_.capacity.size(),
            quantity_type{0});

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            assert(solution.assignment[job] < instance_.capacity.size());
            load[solution.assignment[job]] += instance_.demand[job];
        }

        const auto [minimum, maximum] = std::minmax_element(load.begin(), load.end());
        return static_cast<std::int64_t>(*maximum - *minimum);
    }

private:
    const AssignmentInstance& instance_;
};

} // namespace easylocal::mwe::assignment
