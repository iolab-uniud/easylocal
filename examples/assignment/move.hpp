#pragma once

#include "solution.hpp"

#include <cstddef>

namespace easylocal::mwe::assignment
{

using job_id = std::size_t;

struct Move
{
    job_id job;
    machine_id destination;
};

} // namespace easylocal::mwe::assignment
