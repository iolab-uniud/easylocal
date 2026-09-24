#pragma once

#include <concepts>
#include <ranges>

namespace easylocal::detail
{

template<class Range, class Move>
concept move_input_range_for =
    std::ranges::input_range<Range> &&
    std::same_as<std::ranges::range_value_t<Range>, Move>;

} // namespace easylocal::detail
