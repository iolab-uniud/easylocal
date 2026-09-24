#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

namespace easylocal::mwe::tsp
{

using city_id = std::size_t;
using distance_type = double;

struct Instance
{
    std::size_t city_count{};
    std::vector<distance_type> distances;

    [[nodiscard]]
    auto distance(const city_id from, const city_id to) const noexcept
        -> distance_type
    {
        assert(distances.size() == city_count * city_count);
        assert(from < city_count);
        assert(to < city_count);
        return distances[from * city_count + to];
    }
};

} // namespace easylocal::mwe::tsp
