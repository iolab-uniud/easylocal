#pragma once

#include <easylocal/neighborhood_concepts.hpp>

namespace easylocal::detail
{

template<class Range, class Move>
concept move_input_range_for = easylocal::move_input_range_for<Range, Move>;

} // namespace easylocal::detail
