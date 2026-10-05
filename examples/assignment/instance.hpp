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
        // Signed counts: an unsigned read takes "-1" as a huge count.
        long long job_count{};
        long long machine_count{};
        if (!(input >> job_count >> machine_count) || job_count < 0 || machine_count < 0)
            throw std::runtime_error{"invalid assignment instance header"};

        // One value at a time: a header cannot make it allocate more than the
        // values the text holds.
        AssignmentInstance instance;
        for (long long job = 0; job < job_count; ++job)
        {
            quantity_type demand{};
            if (!(input >> demand) || demand < 0)
                throw std::runtime_error{"invalid assignment demand data"};
            instance.demand.push_back(demand);
        }

        for (long long machine = 0; machine < machine_count; ++machine)
        {
            quantity_type capacity{};
            if (!(input >> capacity) || capacity < 0)
                throw std::runtime_error{"invalid assignment capacity data"};
            instance.capacity.push_back(capacity);
        }

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
