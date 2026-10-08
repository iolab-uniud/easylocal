#pragma once

/// \file
/// The cost value contract.
///
/// Algorithms that need a numeric difference between two already-computed costs
/// use cost::delta(candidate, current); operator- is convenience syntax only
/// and cost types may provide delta via ADL.

#include <easylocal/utils/detail/number_text.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace easylocal::cost
{

namespace detail
{

// An unsigned integer cannot be a cost: the difference of two costs, which
// acceptance criteria and checks compute, would wrap around.
template<class Value>
inline constexpr bool unsigned_cost_v = std::unsigned_integral<std::remove_cv_t<Value>>
    && !std::same_as<std::remove_cv_t<Value>, bool>;

// The domain in which the difference of two arithmetic costs is computed,
// which their own type may not represent: INT_MAX - INT_MIN overflows an int.
// A floating-point cost keeps its type; an integral cost narrower than 64 bits
// widens to a 64-bit integer, which holds every difference, and a 64-bit one
// to double, whose rounding of differences above 2^53 the delta documents.
template<class Cost>
using delta_t = std::conditional_t<
    std::floating_point<Cost>,
    Cost,
    std::conditional_t<(sizeof(Cost) < sizeof(std::int64_t)), std::int64_t, double>>;

} // namespace detail

/// An arithmetic cost: a signed integral or floating-point type other than
/// bool.
///
/// Unsigned integers are not costs: their differences wrap around.
template<class Cost>
concept arithmetic = easylocal::detail::number<Cost> && !detail::unsigned_cost_v<Cost>;

/// The numeric difference `candidate - current` of two arithmetic costs.
///
/// Delta-based acceptance criteria use it; other cost types provide their own
/// `delta` as a free function found by ADL. The difference is computed in a
/// type that represents it: a floating-point cost keeps its own, an integral
/// cost narrower than 64 bits gives a 64-bit integer, a 64-bit one a double,
/// which rounds differences above 2^53.
template<arithmetic Cost>
[[nodiscard]]
constexpr detail::delta_t<Cost> delta(const Cost& candidate, const Cost& current) noexcept
{
    using result = detail::delta_t<Cost>;
    return static_cast<result>(candidate) - static_cast<result>(current);
}

/// A cost type with a numeric difference: `delta(candidate, current)`,
/// convertible to `long double`.
template<class Cost>
concept has_delta =
    requires(const Cost& candidate, const Cost& current) {
        { delta(candidate, current) } -> std::convertible_to<long double>;
    };

/// The zero of a cost type: no violation, no penalty.
///
/// It is Cost{} for types that can be value-initialized (0 for arithmetic
/// costs), the zero of every level for lexicographic and hierarchical costs,
/// and can be given for other types by specializing zero_cost with a static
/// value().
template<class Cost>
struct zero_cost
{
};

/// The zero of a cost that can be value-initialized: Cost{}.
template<std::default_initializable Cost>
struct zero_cost<Cost>
{
    /// Cost{}.
    [[nodiscard]]
    static constexpr Cost value()
    {
        return Cost{};
    }
};

/// A cost type with a zero: `zero_cost<Cost>` has a static `value()`.
template<class Cost>
concept has_zero =
    requires {
        { zero_cost<Cost>::value() } -> std::convertible_to<Cost>;
    };

/// The zero of a cost type, `zero_cost<Cost>::value()`: no violation, no
/// penalty.
template<has_zero Cost>
[[nodiscard]]
constexpr Cost zero()
{
    return zero_cost<Cost>::value();
}

} // namespace easylocal::cost
