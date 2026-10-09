#pragma once

// The cost component of the TSP: the length of the tour.

#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/utils/input_base.hpp>

#include <cassert>
#include <cstddef>
#include <string_view>

namespace tsp
{

class TourLengthComponent : public easylocal::input_base<TspInstance>
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "TourLength";
    }

    using input_base::input_base;

    double evaluate(const Tour& solution) const
    {
        assert(solution.tour.size() == input().city_count);

        const auto n = solution.tour.size();
        double total = 0.0;
        for (std::size_t position = 0; position < n; ++position)
            total += input().distance(
                solution.tour[position],
                solution.tour[(position + 1) % n]);
        return total;
    }
};

} // namespace tsp
