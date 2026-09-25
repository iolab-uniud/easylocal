#pragma once

#include "instance.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace easylocal::mwe::assignment
{

[[nodiscard]]
inline auto load_instance(const std::filesystem::path& path)
    -> AssignmentInstance
{
    std::ifstream input{path};
    if (!input)
    {
        throw std::runtime_error(
            "cannot open assignment instance: " + path.string());
    }

    std::size_t job_count{};
    std::size_t machine_count{};
    if (!(input >> job_count >> machine_count))
    {
        throw std::runtime_error(
            "invalid assignment instance header: " + path.string());
    }

    AssignmentInstance instance{
        .demand = std::vector<quantity_type>(job_count),
        .capacity = std::vector<quantity_type>(machine_count),
    };

    for (auto& demand : instance.demand)
    {
        if (!(input >> demand) || demand < 0)
        {
            throw std::runtime_error(
                "invalid assignment demand data: " + path.string());
        }
    }

    for (auto& capacity : instance.capacity)
    {
        if (!(input >> capacity) || capacity < 0)
        {
            throw std::runtime_error(
                "invalid assignment capacity data: " + path.string());
        }
    }

    if (job_count != 0 && machine_count == 0)
    {
        throw std::runtime_error(
            "assignment instance has jobs but no machines: " + path.string());
    }

    return instance;
}

} // namespace easylocal::mwe::assignment
