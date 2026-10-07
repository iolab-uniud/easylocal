#pragma once

/// \file
/// cost::scalar: a cost as one number, for tools that compare runs by a single
/// value, such as automatic configurators.
///
/// A hierarchical cost is hard times a weight plus soft; a lexicographic cost
/// weighs each value by a power of the weight.

#include <easylocal/cost/hierarchical.hpp>
#include <easylocal/cost/lexicographic.hpp>
#include <easylocal/utils/detail/number_text.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

namespace detail
{

template<class T>
inline constexpr bool scalar_number_v = easylocal::detail::number<T>;

template<class T>
struct scalar_convertible_trait : std::bool_constant<scalar_number_v<T>>
{
};

template<class HardCost, class SoftCost>
struct scalar_convertible_trait<hierarchical<HardCost, SoftCost>>
    : std::bool_constant<
          scalar_convertible_trait<HardCost>::value
          && scalar_convertible_trait<SoftCost>::value>
{
};

template<class... Values>
struct scalar_convertible_trait<lexicographic<Values...>>
    : std::bool_constant<(scalar_convertible_trait<Values>::value && ...)>
{
};

} // namespace detail

/// Costs that cost::scalar turns into one number: numbers, and hierarchical
/// and lexicographic costs of them.
template<class Cost>
concept scalar_convertible = detail::scalar_convertible_trait<Cost>::value;

/// The cost as one number: a number as it is, a hierarchical cost as
/// `hard * weight + soft`, a lexicographic cost of n values as the sum of each
/// value times `weight` to the power of the number of values after it.
///
/// The order of the numbers agrees with the order of the costs when the weight
/// exceeds every lower-priority value, such as the soft cost of any solution,
/// and when the result stays within the 53 bits of a double's mantissa: with
/// the default weight of 10^9, a lexicographic cost of three or more levels,
/// or a hard cost above about 9 * 10^6, loses the lower levels to rounding. A
/// problem then gives its own number, with a scalar_cost(input, cost) hook.
template<scalar_convertible Cost>
[[nodiscard]]
constexpr double scalar(const Cost& cost, const double weight) noexcept
{
    if constexpr (detail::scalar_number_v<Cost>)
        return static_cast<double>(cost);
    else if constexpr (hierarchical_type<Cost>)
        return scalar(cost.hard(), weight) * weight + scalar(cost.soft(), weight);
    else
    {
        return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            double result = 0.0;
            ((result = result * weight + scalar(cost.template get<Index>(), weight)),
                ...);
            return result;
        }(std::make_index_sequence<Cost::levels>{});
    }
}

} // namespace easylocal::cost
