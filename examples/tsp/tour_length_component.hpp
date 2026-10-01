#pragma once

#include "instance.hpp"
#include "solution.hpp"

#include <cassert>
#include <cstddef>

namespace easylocal::mwe::tsp
{

struct TourLengthValue
{
    distance_type total{};

    auto operator==(const TourLengthValue&) const -> bool = default;
};

class TourLengthComponent
{
public:
    explicit TourLengthComponent(const TspInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto evaluate(const Tour& solution) const -> TourLengthValue
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
// explicit aggregator: the tour length itself.
struct TourLengthCost
{
    [[nodiscard]]
    constexpr auto operator()(const TourLengthValue value) const noexcept
        -> distance_type
    {
        return value.total;
    }
};

} // namespace easylocal::mwe::tsp
