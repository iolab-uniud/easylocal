#pragma once

#include "instance.hpp"
#include "solution.hpp"

#include <cassert>
#include <cstddef>

namespace tsp
{

struct TourLengthValue
{
    distance_type total{};

    bool operator==(const TourLengthValue&) const = default;
};

class TourLengthComponent
{
public:
    explicit TourLengthComponent(const TspInstance& instance) : instance_{instance} {}

    TourLengthValue evaluate(const Tour& solution) const
    {
        assert(solution.tour.size() == instance_.city_count);

        if (solution.tour.empty())
        {
            return {};
        }

        distance_type total = 0.0;

        for (std::size_t position = 0; position < solution.tour.size(); ++position)
        {
            const auto from = solution.tour[position];
            const auto to = solution.tour[(position + 1) % solution.tour.size()];

            assert(from < instance_.city_count);
            assert(to < instance_.city_count);
            total += instance_.distance(from, to);
        }

        return TourLengthValue{.total = total};
    }

private:
    const TspInstance& instance_;
};

// TourLengthValue is a domain value, so the cost is obtained through an
// explicit function, cost::apply(TourLengthCost{}, component<...>()): the tour
// length itself.
struct TourLengthCost
{
    constexpr distance_type operator()(TourLengthValue value) const
    {
        return value.total;
    }
};

} // namespace tsp
