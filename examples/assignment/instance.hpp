#pragma once

#include <cstdint>
#include <vector>

namespace easylocal::mwe::assignment
{

using quantity_type = std::int64_t;

struct AssignmentInstance
{
    std::vector<quantity_type> demand;
    std::vector<quantity_type> capacity;
};

} // namespace easylocal::mwe::assignment
