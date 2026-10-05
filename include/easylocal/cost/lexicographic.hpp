#pragma once

/// \file
/// Lexicographically ordered cost: values are compared in declaration order.
///
/// A lexicographic cost has no numeric delta.

#include <easylocal/cost/concepts.hpp>

#include <compare>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

/// A lexicographic cost: its values are compared in the order given, a later
/// one only when the earlier ones are equal.
///
/// It has no numeric delta.
template<class... Values>
class lexicographic
{
    static_assert(
        (!detail::unsigned_cost_v<Values> && ...),
        "a cost cannot be an unsigned integer, whose differences wrap "
        "around: use a signed integer (int, long long) or a floating-point "
        "type");

public:
    /// The number of values.
    static constexpr std::size_t levels = sizeof...(Values);

    /// From its values, in order of priority.
    constexpr explicit lexicographic(Values... values)
        : values_{std::move(values)...}
    {
    }

    /// The value at position `Index`.
    template<std::size_t Index>
    [[nodiscard]]
    constexpr const std::tuple_element_t<Index, std::tuple<Values...>>& get()
        const noexcept
    {
        return std::get<Index>(values_);
    }

    /// Level by level, the first level first.
    auto operator<=>(const lexicographic&) const = default;

private:
    std::tuple<Values...> values_;
};

/// The zero of a lexicographic cost: the zero of every level.
template<class... Values>
    requires (has_zero<Values> && ...)
struct zero_cost<lexicographic<Values...>>
{
    /// The zero of every level.
    [[nodiscard]]
    static constexpr lexicographic<Values...> value()
    {
        return lexicographic<Values...>{zero<Values>()...};
    }
};

/// Whether `T` is a `cost::lexicographic`.
template<class T>
struct is_lexicographic : std::false_type
{
};

/// A `cost::lexicographic` is one.
template<class... Values>
struct is_lexicographic<lexicographic<Values...>> : std::true_type
{
};

/// Whether `T`, without cv and reference qualifiers, is a
/// `cost::lexicographic`.
template<class T>
inline constexpr bool is_lexicographic_v =
    is_lexicographic<std::remove_cvref_t<T>>::value;

/// A `cost::lexicographic`, possibly cv- or reference-qualified.
template<class T>
concept lexicographic_type = is_lexicographic_v<T>;

} // namespace easylocal::cost
