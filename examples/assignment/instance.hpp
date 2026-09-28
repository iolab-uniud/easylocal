#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace easylocal::mwe::assignment
{

using quantity_type = std::int64_t;

struct AssignmentInstance
{
    std::vector<quantity_type> demand;
    std::vector<quantity_type> capacity;

    [[nodiscard]] auto describe() const -> std::string
    {
        return "jobs=" + std::to_string(demand.size()) +
               ", machines=" + std::to_string(capacity.size());
    }
};

} // namespace easylocal::mwe::assignment
