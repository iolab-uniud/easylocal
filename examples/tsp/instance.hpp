#pragma once

// The TSP Input: the number of cities and the full distance matrix, read from a
// text file (the count, then the matrix row by row).

#include <cassert>
#include <cmath>
#include <cstddef>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tsp
{

using city_id = std::size_t;
using distance_type = double;

struct TspInstance
{
    std::size_t city_count{};
    std::vector<distance_type> distances;

    static TspInstance read(std::istream& input)
    {
        std::size_t count{};
        if (!(input >> count) || count < 2)
            throw std::runtime_error("invalid TSP instance header");

        TspInstance instance{
            .city_count = count,
            .distances = std::vector<distance_type>(count * count),
        };
        for (auto& distance : instance.distances)
            if (!(input >> distance) || !std::isfinite(distance) || distance < 0.0)
                throw std::runtime_error("invalid TSP distance data");
        // The 2-opt delta assumes that a reversed segment costs the same.
        for (city_id from = 0; from < count; ++from)
            for (city_id to = 0; to < from; ++to)
                if (instance.distance(from, to) != instance.distance(to, from))
                    throw std::runtime_error("TSP distances are not symmetric");
        return instance;
    }

    distance_type distance(city_id from, city_id to) const
    {
        assert(distances.size() == city_count * city_count);
        assert(from < city_count);
        assert(to < city_count);
        return distances[from * city_count + to];
    }

    std::string describe() const
    {
        return "cities=" + std::to_string(city_count)
            + ", distance_entries=" + std::to_string(distances.size());
    }
};

} // namespace tsp
