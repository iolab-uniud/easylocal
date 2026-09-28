#pragma once

#include <cstddef>
#include <string>

namespace easylocal::mwe::tsp
{

struct SwapCitiesMove
{
    std::size_t first_position;
    std::size_t second_position;

    [[nodiscard]] auto describe() const -> std::string
    {
        return "position " + std::to_string(first_position) +
               " <-> position " + std::to_string(second_position);
    }

    auto operator==(const SwapCitiesMove&) const -> bool = default;
};

} // namespace easylocal::mwe::tsp
