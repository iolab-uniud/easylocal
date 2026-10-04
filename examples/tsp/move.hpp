#pragma once

// The 2-opt move: remove two edges of the tour and reconnect it by reversing
// the segment between them.

#include <cstddef>
#include <string>

namespace tsp
{

struct TwoOptMove
{
    std::size_t first_edge;
    std::size_t second_edge;

    std::string describe() const
    {
        return "edge " + std::to_string(first_edge) + " <-> edge "
            + std::to_string(second_edge);
    }

    bool operator==(const TwoOptMove&) const = default;
};

} // namespace tsp
