#pragma once

#include <compare>
#include <cstdint>

namespace easylocal::mwe::assignment
{

struct Cost
{
    std::int64_t value{};

    auto operator<=>(const Cost&) const = default;
};

} // namespace easylocal::mwe::assignment
