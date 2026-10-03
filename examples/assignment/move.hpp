#pragma once

#include "solution.hpp"

#include <cstddef>
#include <string>

namespace assignment
{

using job_id = std::size_t;

struct ReassignJobMove
{
    job_id job;
    machine_id destination;

    std::string describe() const
    {
        return "job " + std::to_string(job) + " -> machine "
            + std::to_string(destination);
    }

    bool operator==(const ReassignJobMove&) const = default;
};

} // namespace assignment
