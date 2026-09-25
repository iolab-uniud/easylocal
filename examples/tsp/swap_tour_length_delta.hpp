#pragma once

#include "instance.hpp"
#include "solution.hpp"
#include "swap_move.hpp"
#include "tour_length_component.hpp"
#include "tour_length_delta.hpp"

#include <array>
#include <cassert>
#include <cstddef>

namespace easylocal::mwe::tsp
{

class SwapTourLengthDeltaEvaluator
{
public:
    explicit SwapTourLengthDeltaEvaluator(const TspInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto delta_evaluate(
        const Tour& solution,
        const SwapCitiesMove& move) const noexcept -> TourLengthDelta
    {
        assert(solution.tour.size() == instance_.city_count);
        assert(move.first_position < move.second_position);
        assert(move.second_position < solution.tour.size());

        const auto size = solution.tour.size();
        if (size < 2)
        {
            return {};
        }

        const std::array<std::size_t, 4> affected_edges{
            (move.first_position + size - 1) % size,
            move.first_position,
            (move.second_position + size - 1) % size,
            move.second_position,
        };

        distance_type removed = 0.0;
        distance_type added = 0.0;

        for (std::size_t index = 0; index < affected_edges.size(); ++index)
        {
            const auto edge = affected_edges[index];
            bool duplicate = false;

            for (std::size_t previous = 0; previous < index; ++previous)
            {
                duplicate |= affected_edges[previous] == edge;
            }

            if (duplicate)
            {
                continue;
            }

            const auto next = (edge + 1) % size;
            removed += instance_.distance(
                solution.tour[edge],
                solution.tour[next]);
            added += instance_.distance(
                city_after_swap(solution, move, edge),
                city_after_swap(solution, move, next));
        }

        return TourLengthDelta{
            .change = added - removed,
        };
    }

private:
    [[nodiscard]]
    static auto city_after_swap(
        const Tour& solution,
        const SwapCitiesMove& move,
        const std::size_t position) noexcept -> city_id
    {
        if (position == move.first_position)
        {
            return solution.tour[move.second_position];
        }
        if (position == move.second_position)
        {
            return solution.tour[move.first_position];
        }
        return solution.tour[position];
    }

    const TspInstance& instance_;
};

} // namespace easylocal::mwe::tsp
