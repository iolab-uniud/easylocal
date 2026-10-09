#pragma once

// The delta cost component of the swap moves: the change of the tour length,
// from the at most four edges the swap replaces.

#include "instance.hpp"
#include "solution.hpp"
#include "swap_move.hpp"

#include <easylocal/utils/input_base.hpp>

#include <array>
#include <cassert>
#include <cstddef>

namespace tsp
{

class SwapTourLengthDelta : public easylocal::input_base<TspInstance>
{
public:
    using input_base::input_base;

    double delta_evaluate(const Tour& solution, const SwapCitiesMove& move) const
    {
        assert(solution.tour.size() == input().city_count);
        assert(move.first_position < move.second_position);
        assert(move.second_position < solution.tour.size());

        const auto size = solution.tour.size();
        const std::array<std::size_t, 4> affected_edges{
            (move.first_position + size - 1) % size,
            move.first_position,
            (move.second_position + size - 1) % size,
            move.second_position,
        };

        double removed = 0.0;
        double added = 0.0;

        for (std::size_t index = 0; index < affected_edges.size(); ++index)
        {
            const auto edge = affected_edges[index];
            bool duplicate = false;

            for (std::size_t previous = 0; previous < index; ++previous)
                duplicate |= affected_edges[previous] == edge;

            if (duplicate)
                continue;

            const auto next = (edge + 1) % size;
            removed += input().distance(solution.tour[edge], solution.tour[next]);
            added += input().distance(
                city_after_swap(solution, move, edge),
                city_after_swap(solution, move, next));
        }

        return added - removed;
    }

private:
    static city_id city_after_swap(
        const Tour& solution,
        const SwapCitiesMove& move,
        std::size_t position)
    {
        if (position == move.first_position)
            return solution.tour[move.second_position];
        if (position == move.second_position)
            return solution.tour[move.first_position];
        return solution.tour[position];
    }
};

} // namespace tsp
