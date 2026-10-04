#pragma once

// How an assignment instance is read from a text file.

#include "instance.hpp"

#include <cstddef>
#include <istream>
#include <stdexcept>
#include <type_traits>

namespace assignment
{

// The number of jobs and of machines, then the demands, then the capacities.
// EasyLocal finds this function by its name and arguments (ADL) when it reads
// an AssignmentInstance.
inline AssignmentInstance read_input(
    std::type_identity<AssignmentInstance>,
    std::istream& input)
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

} // namespace assignment
