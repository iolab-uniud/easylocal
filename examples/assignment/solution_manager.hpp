#pragma once

#include "instance.hpp"
#include "solution.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <ranges>
#include <vector>

namespace easylocal::mwe::assignment
{

class SolutionManager
{
public:
    using cost_type = std::int64_t;

    explicit SolutionManager(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    auto is_valid(const Solution& solution) const noexcept -> bool
    {
        if (solution.assignment.size() != instance_.demand.size())
        {
            return false;
        }

        return std::ranges::all_of(
            solution.assignment,
            [this](const machine_id machine) {
                return machine < instance_.capacity.size();
            });
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const -> cost_type
    {
        assert(is_valid(solution));

        std::vector<cost_type> load(instance_.capacity.size(), cost_type{0});

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            load[solution.assignment[job]] += instance_.demand[job];
        }

        cost_type total_overload = 0;

        for (std::size_t machine = 0; machine < load.size(); ++machine)
        {
            total_overload += std::max(
                cost_type{0},
                load[machine] - instance_.capacity[machine]);
        }

        return total_overload;
    }

private:
    const Instance& instance_;
};

} // namespace easylocal::mwe::assignment
