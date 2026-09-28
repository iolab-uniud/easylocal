#pragma once

#include "instance.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace easylocal::mwe::tsp
{

[[nodiscard]]
inline auto load_instance(const std::filesystem::path& path) -> TspInstance
{
    std::ifstream input{path};
    if (!input)
    {
        throw std::runtime_error("cannot open TSP instance: " + path.string());
    }

    try
    {
        return TspInstance::read(input);
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(
            "cannot read TSP instance '" + path.string() + "': " + error.what());
    }
}

} // namespace easylocal::mwe::tsp
