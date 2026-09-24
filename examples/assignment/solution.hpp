#pragma once

#include <cstddef>
#include <vector>

namespace easylocal::mwe::assignment
{

using machine_id = std::size_t;

struct AssignmentSolution
{
    std::vector<machine_id> assignment;
};

} // namespace easylocal::mwe::assignment
