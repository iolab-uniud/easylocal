#pragma once

#include <cstddef>

namespace easylocal::mwe::tsp
{

struct TwoOptMove
{
    std::size_t first_edge;
    std::size_t second_edge;

    auto operator==(const TwoOptMove&) const -> bool = default;
};

} // namespace easylocal::mwe::tsp
