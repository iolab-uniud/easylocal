#pragma once

#include <cstddef>
#include <string>

namespace easylocal::mwe::tsp
{

struct TwoOptMove
{
    std::size_t first_edge;
    std::size_t second_edge;

    [[nodiscard]] auto describe() const -> std::string
    {
        return "edge " + std::to_string(first_edge) +
               " <-> edge " + std::to_string(second_edge);
    }

    auto operator==(const TwoOptMove&) const -> bool = default;
};

} // namespace easylocal::mwe::tsp
