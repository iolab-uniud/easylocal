#pragma once

// The cost components of the assignment problem: the capacity violation
// (hard) and the load imbalance (soft).

#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/utils/input_base.hpp>

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

class CapacityCostComponent : public easylocal::input_base<AssignmentInstance>
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "Capacity";
    }

    using input_base::input_base;

    CapacityValue evaluate(const AssignmentSolution& solution) const
    {
        assert(solution.assignment.size() == input().demand.size());

        std::vector<quantity_type> load(input().capacity.size(), quantity_type{0});

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            assert(solution.assignment[job] < input().capacity.size());
            load[solution.assignment[job]] += input().demand[job];
        }

        CapacityValue value{};

        for (std::size_t machine = 0; machine < load.size(); ++machine)
        {
            const auto machine_overload =
                overload(load[machine], input().capacity[machine]);

            value.overloaded_machines += machine_overload > 0 ? 1 : 0;
            value.total_overload += machine_overload;
        }

        return value;
    }
};

class LoadImbalanceCostComponent : public easylocal::input_base<AssignmentInstance>
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "LoadImbalance";
    }

    using input_base::input_base;

    // The largest machine load minus the smallest one: a plain number.
    std::int64_t evaluate(const AssignmentSolution& solution) const
    {
        assert(solution.assignment.size() == input().demand.size());

        if (input().capacity.empty())
            return 0;

        std::vector<quantity_type> load(input().capacity.size(), quantity_type{0});

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            assert(solution.assignment[job] < input().capacity.size());
            load[solution.assignment[job]] += input().demand[job];
        }

        const auto [minimum, maximum] = std::ranges::minmax_element(load);
        return *maximum - *minimum;
    }
};

} // namespace assignment
