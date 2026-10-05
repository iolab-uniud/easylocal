#pragma once

/// \file
/// cost::tolerance: comparisons of costs that forgive the rounding errors of
/// floating-point arithmetic, such as the drift of a cost updated by deltas.
///
/// Two floating-point values are approximately equal when they differ by at
/// most `max(absolute, relative * max(|a|, |b|))`; integers compare exactly;
/// lexicographic, hierarchical and pareto costs compare level by level. The
/// checks of easylocal::testing and check(app) use it by default, and
/// cost::approximately gives it to the search.

#include <easylocal/config/parameters.hpp>
#include <easylocal/cost/hierarchical.hpp>
#include <easylocal/cost/lexicographic.hpp>
#include <easylocal/cost/pareto.hpp>
#include <easylocal/utils/detail/number_text.hpp>
#include <easylocal/utils/limit.hpp>

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

namespace detail
{

template<class T>
inline constexpr bool cost_model_v =
    is_hierarchical_v<T> || is_lexicographic_v<T> || is_pareto_v<T>;

} // namespace detail

/// Values that approximately_equal compares: two numbers, two lexicographic,
/// hierarchical or pareto costs of the same type, or values with `==`.
template<class Left, class Right>
concept approximately_equality_comparable =
    (easylocal::detail::number<Left> && easylocal::detail::number<Right>)
    || (std::same_as<Left, Right> && detail::cost_model_v<Left>)
    || std::equality_comparable_with<Left, Right>;

/// The tolerance of an approximate comparison of costs; called on two values,
/// whether they are approximately equal (see approximately_equal).
///
/// Two floating-point values are equal within it when they differ by at most
/// the larger of `absolute` and `relative` times the larger magnitude.
struct tolerance
{
    /// The tolerance relative to the larger magnitude of the two values
    /// (default 1e-9).
    double relative{1e-9};
    /// The tolerance near zero, in the unit of the cost (default 1e-9).
    double absolute{1e-9};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"relative", &tolerance::relative>(
                "Tolerance relative to the larger magnitude of two costs",
                config::range(0.0, easylocal::unlimited)),
            config::field<"absolute", &tolerance::absolute>(
                "Tolerance near zero, in the unit of the cost",
                config::range(0.0, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }

    /// Whether `lhs` and `rhs` are equal within the tolerance:
    /// approximately_equal(lhs, rhs, *this).
    template<class Left, class Right>
        requires approximately_equality_comparable<Left, Right>
    [[nodiscard]]
    constexpr bool operator()(const Left& lhs, const Right& rhs) const;
};

namespace detail
{

// Two numbers within the tolerance, when either is floating point; exactly
// equal otherwise. Infinities are equal only to themselves, NaN to nothing.
template<class Left, class Right>
[[nodiscard]]
constexpr bool numbers_approximately_equal(
    const Left lhs,
    const Right rhs,
    const tolerance& within) noexcept
{
    if constexpr (std::floating_point<Left> || std::floating_point<Right>)
    {
        const auto left = static_cast<long double>(lhs);
        const auto right = static_cast<long double>(rhs);
        if (left == right)
            return true;
        if (!std::isfinite(left) || !std::isfinite(right))
            return false;
        const auto difference = std::abs(left - right);
        const auto scale = std::max(std::abs(left), std::abs(right));
        return difference <= std::max(
                   static_cast<long double>(within.absolute),
                   static_cast<long double>(within.relative) * scale);
    }
    else
        return std::cmp_equal(lhs, rhs);
}

// The order of two values that compare with <=>, or else with < alone.
template<class Value>
[[nodiscard]]
constexpr std::partial_ordering exact_order(const Value& lhs, const Value& rhs)
{
    if constexpr (std::three_way_comparable<Value, std::partial_ordering>)
        return static_cast<std::partial_ordering>(lhs <=> rhs);
    else
    {
        if (lhs < rhs)
            return std::partial_ordering::less;
        if (rhs < lhs)
            return std::partial_ordering::greater;
        return std::partial_ordering::equivalent;
    }
}

} // namespace detail

/// A cost that approximate_compare orders: a number, a lexicographic,
/// hierarchical or pareto cost of such costs, or a type with `<=>` or `<`.
template<class Cost>
concept approximately_comparable = easylocal::detail::number<Cost>
    || detail::cost_model_v<Cost> || requires(const Cost& lhs, const Cost& rhs) {
           { lhs < rhs } -> std::convertible_to<bool>;
       };

/// The order of two costs, with floating-point values that are equal within
/// `within` taken as equivalent.
///
/// Numbers compare with `<=>`, after the test of equality; a lexicographic cost
/// level by level, the first that is not equivalent deciding; a hierarchical
/// cost by its hard cost, then by its soft cost; a pareto cost by dominance,
/// each objective compared within the tolerance; any other type with its own
/// `<=>`, or `<`.
template<approximately_comparable Cost>
[[nodiscard]]
constexpr std::partial_ordering approximate_compare(
    const Cost& lhs,
    const Cost& rhs,
    const tolerance& within = {})
{
    if constexpr (easylocal::detail::number<Cost>)
    {
        if (detail::numbers_approximately_equal(lhs, rhs, within))
            return std::partial_ordering::equivalent;
        return static_cast<std::partial_ordering>(lhs <=> rhs);
    }
    else if constexpr (is_hierarchical_v<Cost>)
    {
        const auto hard = approximate_compare(lhs.hard(), rhs.hard(), within);
        if (hard != 0)
            return hard;
        return approximate_compare(lhs.soft(), rhs.soft(), within);
    }
    else if constexpr (is_lexicographic_v<Cost>)
    {
        return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            auto order = std::partial_ordering::equivalent;
            ((order = order == 0
                     ? approximate_compare(
                           lhs.template get<Index>(),
                           rhs.template get<Index>(),
                           within)
                     : order),
                ...);
            return order;
        }(std::make_index_sequence<Cost::levels>{});
    }
    else if constexpr (is_pareto_v<Cost>)
    {
        return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            bool lhs_better = false;
            bool rhs_better = false;
            bool unordered = false;
            const auto objective = [&](const std::partial_ordering order) {
                if (order < 0)
                    lhs_better = true;
                else if (order > 0)
                    rhs_better = true;
                else if (order != 0)
                    unordered = true;
            };
            (objective(approximate_compare(
                 lhs.template get<Index>(),
                 rhs.template get<Index>(),
                 within)),
                ...);
            if (unordered || (lhs_better && rhs_better))
                return std::partial_ordering::unordered;
            if (lhs_better)
                return std::partial_ordering::less;
            if (rhs_better)
                return std::partial_ordering::greater;
            return std::partial_ordering::equivalent;
        }(std::make_index_sequence<Cost::levels>{});
    }
    else
        return detail::exact_order(lhs, rhs);
}

/// Whether two values are equal within `within`: numbers when either is
/// floating point (integers exactly), lexicographic, hierarchical and pareto
/// costs level by level, other values with `==`.
///
/// It is not transitive: a chain of values each close to the next may end far
/// from where it began.
template<class Left, class Right>
    requires approximately_equality_comparable<Left, Right>
[[nodiscard]]
constexpr bool approximately_equal(
    const Left& lhs,
    const Right& rhs,
    const tolerance& within = {})
{
    if constexpr (easylocal::detail::number<Left> && easylocal::detail::number<Right>)
        return detail::numbers_approximately_equal(lhs, rhs, within);
    else if constexpr (std::same_as<Left, Right> && detail::cost_model_v<Left>)
        return approximate_compare(lhs, rhs, within) == 0;
    else
        return static_cast<bool>(lhs == rhs);
}

template<class Left, class Right>
    requires approximately_equality_comparable<Left, Right>
constexpr bool tolerance::operator()(const Left& lhs, const Right& rhs) const
{
    return approximately_equal(lhs, rhs, *this);
}

} // namespace easylocal::cost
