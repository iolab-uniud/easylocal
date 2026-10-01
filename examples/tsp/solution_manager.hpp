#pragma once

#include "instance.hpp"
#include "solution.hpp"
#include "tour_length_component.hpp"

#include <easylocal/helpers/service_base.hpp>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <random>

namespace easylocal::mwe::tsp
{

class TspSolutionManager
    : public easylocal::solution_manager_base<TspInstance, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    auto initial_solution() const -> Tour
    {
        Tour solution;
        solution.tour.resize(input_.city_count);
        std::iota(solution.tour.begin(), solution.tour.end(), city_id{0});
        return solution;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const -> Tour
    {
        auto solution = initial_solution();
        std::shuffle(solution.tour.begin(), solution.tour.end(), rng);
        return solution;
    }

    [[nodiscard]]
    auto is_valid(const Tour& solution) const noexcept -> bool
    {
        if (solution.tour.size() != input_.city_count)
        {
            return false;
        }

        for (std::size_t first = 0; first < solution.tour.size(); ++first)
        {
            if (solution.tour[first] >= input_.city_count)
            {
                return false;
            }

            for (std::size_t second = first + 1;
                 second < solution.tour.size();
                 ++second)
            {
                if (solution.tour[first] == solution.tour[second])
                {
                    return false;
                }
            }
        }

        return true;
    }

};

} // namespace easylocal::mwe::tsp
