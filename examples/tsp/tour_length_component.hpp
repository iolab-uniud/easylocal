#pragma once

// The cost component of the TSP: the length of the tour.

#include "instance.hpp"
#include "solution.hpp"

#include <cassert>
#include <cstddef>
#include <string_view>

namespace tsp
{

class TourLengthComponent
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "TourLength";
    }

    explicit TourLengthComponent(const TspInstance& instance) : instance_{instance} {}

    double evaluate(const Tour& solution) const
    {
        assert(solution.tour.size() == instance_.city_count);

        const auto n = solution.tour.size();
        double total = 0.0;
        for (std::size_t position = 0; position < n; ++position)
            total += instance_.distance(
                solution.tour[position],
                solution.tour[(position + 1) % n]);
        return total;
    }

private:
    const TspInstance& instance_;
};

} // namespace tsp
