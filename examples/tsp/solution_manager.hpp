#pragma once

// The SolutionManager of the TSP: initial and random tours, and their validity.

#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/helpers/solution_manager.hpp>

#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

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
        std::vector<bool> seen(input().city_count, false);
        for (const auto city : solution.tour)
        {
            if (city >= input().city_count || seen[city])
                return false;
            seen[city] = true;
        }
        return true;
    }
};

} // namespace tsp
