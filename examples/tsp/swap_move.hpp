#pragma once

// The swap move: exchange the cities visited at two positions of the tour.

#include <cstddef>
#include <string>

namespace tsp
{

struct SwapCitiesMove
{
    std::size_t first_position;
    std::size_t second_position;

    std::string describe() const
    {
        return "position " + std::to_string(first_position) + " <-> position "
            + std::to_string(second_position);
    }

    bool operator==(const SwapCitiesMove&) const = default;
};

} // namespace tsp
