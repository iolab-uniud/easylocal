#pragma once

#include "instance.hpp"
#include "solution.hpp"
#include "tour_length_component.hpp"

#include <cstddef>

namespace easylocal::mwe::tsp
{

class SolutionManager
{
public:
    using instance_type = Instance;
    using solution_type = Solution;

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
        if (solution.tour.size() != instance_.city_count)
        {
            return false;
        }

        for (std::size_t first = 0; first < solution.tour.size(); ++first)
        {
            if (solution.tour[first] >= instance_.city_count)
            {
                return false;
            }

            for (std::size_t second = first + 1;
                 second < solution.tour.size();
                 ++second)
            {
                if (solution.tour[first] == solution.tour[second])
                {
                    return false;
                }
            }
        }

        return true;
    }

    [[nodiscard]]
    constexpr auto aggregate(const TourLengthValue& length) const noexcept
        -> distance_type
    {
        return length.total;
    }

private:
    const Instance& instance_;
};

} // namespace easylocal::mwe::tsp
