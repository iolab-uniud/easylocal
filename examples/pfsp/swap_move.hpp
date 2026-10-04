#pragma once

// The swap move of the PFSP: exchange the jobs at two positions.

#include "instance.hpp"

#include <cstddef>
#include <string>

namespace pfsp
{

// The swap of the jobs at two positions, with the jobs it moves: the tabu
// definitions are on the jobs, not on the positions.
struct SwapJobsMove
{
    std::size_t first_position{};
    std::size_t second_position{};
    job_id first_job{};
    job_id second_job{};

    std::string describe() const
    {
        return "job " + std::to_string(first_job) + " (position "
            + std::to_string(first_position) + ") <-> job " + std::to_string(second_job)
            + " (position " + std::to_string(second_position) + ")";
    }

    bool operator==(const SwapJobsMove&) const = default;
};

} // namespace pfsp
