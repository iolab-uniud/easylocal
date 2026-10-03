#pragma once

#include "instance.hpp"
#include "solution.hpp"
#include "tour_length_component.hpp"

#include <easylocal/helpers/solution_manager.hpp>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <random>

namespace tsp
{

class TspSolutionManager : public easylocal::solution_manager_base<TspInstance, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    Tour initial_solution() const
    {
        Tour solution;
        solution.tour.resize(input().city_count);
        std::ranges::iota(solution.tour, city_id{0});
        return solution;
    }

    template<std::uniform_random_bit_generator RNG>
    Tour random_solution(RNG& rng) const
    {
        auto solution = initial_solution();
        std::shuffle(solution.tour.begin(), solution.tour.end(), rng);
        return solution;
    }

    bool is_valid(const Tour& solution) const
    {
        if (solution.tour.size() != input().city_count)
            return false;

        for (std::size_t first = 0; first < solution.tour.size(); ++first)
        {
            if (solution.tour[first] >= input().city_count)
                return false;

            for (std::size_t second = first + 1; second < solution.tour.size(); ++second)
                if (solution.tour[first] == solution.tour[second])
                    return false;
        }

        return true;
    }
};

} // namespace tsp
