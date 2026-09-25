#pragma once

#include "instance.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

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

    std::size_t city_count{};
    if (!(input >> city_count) || city_count < 2)
    {
        throw std::runtime_error("invalid TSP instance header: " + path.string());
    }

    TspInstance instance{
        .city_count = city_count,
        .distances = std::vector<distance_type>(city_count * city_count),
    };

    for (auto& distance : instance.distances)
    {
        if (!(input >> distance) || !std::isfinite(distance) || distance < 0.0)
        {
            throw std::runtime_error(
                "invalid TSP distance data: " + path.string());
        }
    }

    return instance;
}

} // namespace easylocal::mwe::tsp
