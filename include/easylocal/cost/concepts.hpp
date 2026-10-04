#pragma once

/// \file
/// The cost value contract.
///
/// Algorithms that need a numeric difference between two already-computed costs
/// use cost::delta(candidate, current); operator- is convenience syntax only
/// and cost types may provide delta via ADL.

#include <easylocal/utils/detail/number_text.hpp>

#include <concepts>
#include <type_traits>

namespace easylocal::cost
{

/// An arithmetic cost: an integral or floating-point type other than bool.
template<class Cost>
concept arithmetic = easylocal::detail::number<Cost>;

/// The numeric difference `candidate - current` of two arithmetic costs.
///
/// Delta-based acceptance criteria use it; other cost types provide their own
/// `delta` as a free function found by ADL.
template<arithmetic Cost>
[[nodiscard]]
constexpr auto delta(const Cost& candidate, const Cost& current)
    noexcept(noexcept(candidate - current))
{
    return candidate - current;
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

template<std::default_initializable Cost>
struct zero_cost<Cost>
{
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
