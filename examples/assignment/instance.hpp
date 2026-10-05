#pragma once

// The input of the assignment problem: the demand of each job and the
// capacity of each machine.

#include <cstddef>
#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

namespace assignment
{

using quantity_type = std::int64_t;

struct AssignmentInstance
{
    std::vector<quantity_type> demand;
    std::vector<quantity_type> capacity;

    // An instance file: the number of jobs and of machines, then the demands,
    // then the capacities. EasyLocal reads an AssignmentInstance with it.
    static AssignmentInstance read(std::istream& input)
    {
        std::size_t job_count{};
        std::size_t machine_count{};
        if (!(input >> job_count >> machine_count))
            throw std::runtime_error{"invalid assignment instance header"};

        AssignmentInstance instance{
            .demand = std::vector<quantity_type>(job_count),
            .capacity = std::vector<quantity_type>(machine_count),
        };

        for (auto& demand : instance.demand)
            if (!(input >> demand) || demand < 0)
                throw std::runtime_error{"invalid assignment demand data"};

        for (auto& capacity : instance.capacity)
            if (!(input >> capacity) || capacity < 0)
                throw std::runtime_error{"invalid assignment capacity data"};

        if (job_count != 0 && machine_count == 0)
            throw std::runtime_error{"assignment instance has jobs but no machines"};

        return instance;
    }

    std::string describe() const
    {
        return "jobs=" + std::to_string(demand.size())
            + ", machines=" + std::to_string(capacity.size());
    }
};

} // namespace assignment
