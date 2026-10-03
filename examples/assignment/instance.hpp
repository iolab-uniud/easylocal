#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assignment
{

using quantity_type = std::int64_t;

struct AssignmentInstance
{
    std::vector<quantity_type> demand;
    std::vector<quantity_type> capacity;

    std::string describe() const
    {
        return "jobs=" + std::to_string(demand.size())
            + ", machines=" + std::to_string(capacity.size());
    }
};

} // namespace assignment
