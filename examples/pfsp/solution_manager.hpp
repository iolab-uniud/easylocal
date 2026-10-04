#pragma once

// The SolutionManager of the PFSP: initial and random orders, their validity
// and their hash.

#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/utils/hash.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace pfsp
{

class PfspSolutionManager
    : public easylocal::solution_manager_base<PfspInstance, Schedule>
{
public:
    using solution_manager_base::solution_manager_base;

    Schedule initial_solution() const
    {
        Schedule solution;
        solution.order.resize(input().job_count);
        std::ranges::iota(solution.order, job_id{0});
        return solution;
    }

    template<std::uniform_random_bit_generator RNG>
    Schedule random_solution(RNG& rng) const
    {
        auto solution = initial_solution();
        std::shuffle(solution.order.begin(), solution.order.end(), rng);
        return solution;
    }

    bool is_valid(const Schedule& solution) const
    {
        if (solution.order.size() != input().job_count)
            return false;
        std::vector<bool> seen(input().job_count, false);
        for (const auto job : solution.order)
        {
            if (job >= input().job_count || seen[job])
                return false;
            seen[job] = true;
        }
        return true;
    }

    // The solution identity, for the features that recognize a schedule met
    // before (a reactive tabu list, search trajectories): the order of the
    // jobs. std::vector has no std::hash, so the SolutionManager gives it.
    std::uint64_t hash(const Schedule& solution) const
    {
        return easylocal::hash_range(solution.order);
    }
};

} // namespace pfsp
