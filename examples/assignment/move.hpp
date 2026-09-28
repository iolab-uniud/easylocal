#pragma once

#include "solution.hpp"

#include <cstddef>
#include <string>

namespace easylocal::mwe::assignment
{

using job_id = std::size_t;

struct ReassignJobMove
{
    job_id job;
    machine_id destination;

    [[nodiscard]] auto describe() const -> std::string
    {
        return "job " + std::to_string(job) + " -> machine " +
               std::to_string(destination);
    }

    auto operator==(const ReassignJobMove&) const -> bool = default;
};

} // namespace easylocal::mwe::assignment
