#pragma once

#include <concepts>
#include <type_traits>

// The cost value contract. Algorithms that need a numeric difference between
// two already-computed costs use cost::delta(candidate, current); operator- is
// convenience syntax only and cost types may provide delta via ADL.
namespace easylocal::cost
{

template<class Cost>
concept arithmetic =
    (std::integral<std::remove_cv_t<Cost>> ||
     std::floating_point<std::remove_cv_t<Cost>>) &&
    (!std::same_as<std::remove_cv_t<Cost>, bool>);

template<arithmetic Cost>
[[nodiscard]]
constexpr auto delta(const Cost& candidate, const Cost& current)
    noexcept(noexcept(candidate - current))
{
    return candidate - current;
}

template<class Cost>
concept has_delta =
    requires(const Cost& candidate, const Cost& current) {
        { delta(candidate, current) } -> std::convertible_to<long double>;
    };

// The zero of a cost type: no violation, no penalty. It is Cost{} for types
// that can be value-initialized (0 for arithmetic costs), the zero of every
// level for lexicographic and hierarchical costs, and can be given for other
// types by specializing zero_cost with a static value().
template<class Cost>
struct zero_cost
{
};

template<std::default_initializable Cost>
struct zero_cost<Cost>
{
    [[nodiscard]]
    static constexpr auto value() -> Cost
    {
        return Cost{};
    }
};

template<class Cost>
concept has_zero =
    requires {
        { zero_cost<Cost>::value() } -> std::convertible_to<Cost>;
    };

template<has_zero Cost>
[[nodiscard]]
constexpr auto zero() -> Cost
{
    return zero_cost<Cost>::value();
}

} // namespace easylocal::cost
