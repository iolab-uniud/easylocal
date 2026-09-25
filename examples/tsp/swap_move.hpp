#pragma once

#include <cstddef>

namespace easylocal::mwe::tsp
{

struct SwapCitiesMove
{
    std::size_t first_position;
    std::size_t second_position;

    auto operator==(const SwapCitiesMove&) const -> bool = default;
};

} // namespace easylocal::mwe::tsp
