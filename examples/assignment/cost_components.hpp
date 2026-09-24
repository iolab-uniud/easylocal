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
    const Instance& instance,
    const Solution& solution,
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
    using value_type = CapacityValue;

    explicit CapacityCostComponent(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const -> value_type
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

        value_type value{};

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
    const Instance& instance_;
};

} // namespace easylocal::mwe::assignment
