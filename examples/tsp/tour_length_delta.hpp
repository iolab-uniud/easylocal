#pragma once

// The delta cost component of the 2-opt moves: the change of the tour length,
// from the two edges the move replaces.

#include "instance.hpp"
#include "move.hpp"
#include "solution.hpp"

#include <easylocal/utils/input_base.hpp>

#include <cassert>

namespace tsp
{

class TwoOptTourLengthDelta : public easylocal::input_base<TspInstance>
{
public:
    using input_base::input_base;

    // The two edges removed and the two added: the reversed segment between
    // them costs the same, since TspInstance::read accepts only symmetric
    // distances.
    double delta_evaluate(const Tour& solution, const TwoOptMove& move) const
    {
        assert(solution.tour.size() == input().city_count);
        assert(move.first_edge < move.second_edge);
        assert(move.second_edge < solution.tour.size());
        assert(move.second_edge != move.first_edge + 1);
        assert(!(move.first_edge == 0 && move.second_edge + 1 == solution.tour.size()));

        const auto first = solution.tour[move.first_edge];
        const auto first_next =
            solution.tour[(move.first_edge + 1) % solution.tour.size()];
        const auto second = solution.tour[move.second_edge];
        const auto second_next =
            solution.tour[(move.second_edge + 1) % solution.tour.size()];

        const auto removed =
            input().distance(first, first_next) + input().distance(second, second_next);
        const auto added =
            input().distance(first, second) + input().distance(first_next, second_next);

        return added - removed;
    }
};

} // namespace tsp
