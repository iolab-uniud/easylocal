#pragma once

#include "instance.hpp"
#include "move.hpp"
#include "solution.hpp"
#include "tour_length_component.hpp"

#include <cassert>
#include <cstddef>

namespace easylocal::mwe::tsp
{

struct TourLengthDelta
{
    distance_type change{};

    auto operator==(const TourLengthDelta&) const -> bool = default;
};

[[nodiscard]]
constexpr auto operator+(
    const TourLengthValue value,
    const TourLengthDelta delta) noexcept -> TourLengthValue
{
    return TourLengthValue{
        .total = value.total + delta.change,
    };
}

class TwoOptTourLengthDeltaEvaluator
{
public:
    explicit TwoOptTourLengthDeltaEvaluator(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto delta_evaluate(
        const Solution& solution,
        const TwoOptMove& move) const noexcept -> TourLengthDelta
    {
        assert(solution.tour.size() == instance_.city_count);
        assert(move.first_edge < move.second_edge);
        assert(move.second_edge < solution.tour.size());
        assert(move.second_edge != move.first_edge + 1);
        assert(!(move.first_edge == 0 &&
                 move.second_edge + 1 == solution.tour.size()));

        const auto first = solution.tour[move.first_edge];
        const auto first_next =
            solution.tour[(move.first_edge + 1) % solution.tour.size()];
        const auto second = solution.tour[move.second_edge];
        const auto second_next =
            solution.tour[(move.second_edge + 1) % solution.tour.size()];

        const auto removed =
            instance_.distance(first, first_next) +
            instance_.distance(second, second_next);
        const auto added =
            instance_.distance(first, second) +
            instance_.distance(first_next, second_next);

        return TourLengthDelta{
            .change = added - removed,
        };
    }

private:
    const Instance& instance_;
};

} // namespace easylocal::mwe::tsp
