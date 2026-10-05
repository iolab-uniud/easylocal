#pragma once

// The shapes of the cost types of easylocal::cost, recognized by their members
// (the trace layer does not depend on cost/): a cost::lexicographic or a
// cost::pareto has levels and get<Index>(), a cost::hierarchical hard() and
// soft(). The ELTR and JSON cost writers encode a cost by its shape.

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace easylocal::trace::detail
{

// A number cost: an integer or a floating-point value, not a bool.
template<class Cost>
concept number_cost =
    (std::integral<Cost> || std::floating_point<Cost>) && !std::same_as<Cost, bool>;

// A cost of numbered levels, read with get<Index>().
template<class Cost>
concept leveled_shape = requires {
    { Cost::levels } -> std::convertible_to<std::size_t>;
};

template<class Cost, std::size_t Index>
using level_type =
    std::remove_cvref_t<decltype(std::declval<const Cost&>().template get<Index>())>;

// A cost of a hard and a soft part.
template<class Cost>
concept hard_soft_shape = requires(const Cost& cost) {
    cost.hard();
    cost.soft();
};

template<class Cost>
using hard_type = std::remove_cvref_t<decltype(std::declval<const Cost&>().hard())>;

template<class Cost>
using soft_type = std::remove_cvref_t<decltype(std::declval<const Cost&>().soft())>;

} // namespace easylocal::trace::detail
