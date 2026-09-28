#pragma once

#include "instance.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <istream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace easylocal::mwe::assignment
{

[[nodiscard]]
inline auto read_assignment_instance(std::istream& input) -> AssignmentInstance
{
    std::size_t job_count{};
    std::size_t machine_count{};
    if (!(input >> job_count >> machine_count))
    {
        throw std::runtime_error{"invalid assignment instance header"};
    }

    AssignmentInstance instance{
        .demand = std::vector<quantity_type>(job_count),
        .capacity = std::vector<quantity_type>(machine_count),
    };

    for (auto& demand : instance.demand)
    {
        if (!(input >> demand) || demand < 0)
        {
            throw std::runtime_error{"invalid assignment demand data"};
        }
    }

    for (auto& capacity : instance.capacity)
    {
        if (!(input >> capacity) || capacity < 0)
        {
            throw std::runtime_error{"invalid assignment capacity data"};
        }
    }

    if (job_count != 0 && machine_count == 0)
    {
        throw std::runtime_error{"assignment instance has jobs but no machines"};
    }

    return instance;
}

[[nodiscard]]
inline auto read_input(
    std::type_identity<AssignmentInstance>,
    std::istream& input) -> AssignmentInstance
{
    return read_assignment_instance(input);
}

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

    try
    {
        return read_assignment_instance(input);
    }
    catch (const std::runtime_error& error)
    {
        throw std::runtime_error{
            std::string{error.what()} + ": " + path.string()};
    }
}

} // namespace easylocal::mwe::assignment
