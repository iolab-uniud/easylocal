#pragma once

// The cost components of the assignment problem: the capacity violation
// (hard) and the load imbalance (soft).

#include "instance.hpp"
#include "solution.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace assignment
{

// The value of CapacityCostComponent: a component value may be a struct.
struct CapacityValue
{
    std::int64_t overloaded_machines{};
    std::int64_t total_overload{};

    bool operator==(const CapacityValue&) const = default;

    // The value as --report and the TextUI show it.
    std::string describe() const
    {
        return std::to_string(total_overload) + " units over capacity on "
            + std::to_string(overloaded_machines) + " machines";
    }
};

// The part of a machine's load beyond its capacity.
inline quantity_type overload(quantity_type load, quantity_type capacity)
{
    return std::max(quantity_type{0}, load - capacity);
}

// No delta cost component is bound to these components: the change of a machine's
// load needs the loads, and computing them means scanning every job, which is
// what a full evaluation does. EasyLocal then evaluates each move on a
// candidate solution, which costs the same and needs no code.

class CapacityCostComponent
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "Capacity";
    }

    explicit CapacityCostComponent(const AssignmentInstance& instance)
        : instance_{instance}
    {
    }

    CapacityValue evaluate(const AssignmentSolution& solution) const
    {
        assert(solution.assignment.size() == instance_.demand.size());

        std::vector<quantity_type> load(instance_.capacity.size(), quantity_type{0});

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            assert(solution.assignment[job] < instance_.capacity.size());
            load[solution.assignment[job]] += instance_.demand[job];
        }

        CapacityValue value{};

        for (std::size_t machine = 0; machine < load.size(); ++machine)
        {
            const auto machine_overload =
                overload(load[machine], instance_.capacity[machine]);

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
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "LoadImbalance";
    }

    explicit LoadImbalanceCostComponent(const AssignmentInstance& instance)
        : instance_{instance}
    {
    }

    // The largest machine load minus the smallest one: a plain number.
    std::int64_t evaluate(const AssignmentSolution& solution) const
    {
        assert(solution.assignment.size() == instance_.demand.size());

        if (instance_.capacity.empty())
            return 0;

        std::vector<quantity_type> load(instance_.capacity.size(), quantity_type{0});

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            assert(solution.assignment[job] < instance_.capacity.size());
            load[solution.assignment[job]] += instance_.demand[job];
        }

        const auto [minimum, maximum] = std::ranges::minmax_element(load);
        return *maximum - *minimum;
    }

private:
    const AssignmentInstance& instance_;
};

} // namespace assignment
