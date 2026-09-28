#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <istream>
#include <string>
#include <stdexcept>
#include <vector>

namespace easylocal::mwe::tsp
{

using city_id = std::size_t;
using distance_type = double;

struct TspInstance
{
    std::size_t city_count{};
    std::vector<distance_type> distances;

    [[nodiscard]]
    static auto read(std::istream& input) -> TspInstance
    {
        std::size_t count{};
        if (!(input >> count) || count < 2)
        {
            throw std::runtime_error("invalid TSP instance header");
        }

        TspInstance instance{
            .city_count = count,
            .distances = std::vector<distance_type>(count * count),
        };
        for (auto& distance : instance.distances)
        {
            if (!(input >> distance) || !std::isfinite(distance) || distance < 0.0)
            {
                throw std::runtime_error("invalid TSP distance data");
            }
        }
        return instance;
    }

    [[nodiscard]]
    auto distance(const city_id from, const city_id to) const noexcept
        -> distance_type
    {
        assert(distances.size() == city_count * city_count);
        assert(from < city_count);
        assert(to < city_count);
        return distances[from * city_count + to];
    }

    [[nodiscard]] auto describe() const -> std::string
    {
        return "cities=" + std::to_string(city_count) +
               ", distance_entries=" + std::to_string(distances.size());
    }
};

} // namespace easylocal::mwe::tsp
