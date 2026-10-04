#pragma once

/// \file
/// Hierarchical hard/soft cost: the hard branch has strict priority, soft is
/// compared only when hard is equivalent. Each branch may itself be scalar or
/// structured (e.g. cost::lexicographic).
///
/// delta() preserves the hard level: hard better -> -inf, hard worse -> +inf,
/// hard equivalent -> soft delta. This lets delta-based acceptance work on the
/// full cost without ever accepting a hard degradation.

#include <easylocal/cost/concepts.hpp>

#include <compare>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

template<class HardCost, class SoftCost>
class hierarchical
{
public:
    using hard_cost_type = HardCost;
    using soft_cost_type = SoftCost;

    constexpr explicit hierarchical(HardCost hard, SoftCost soft)
        : hard_{std::move(hard)},
          soft_{std::move(soft)}
    {
    }

    [[nodiscard]]
    constexpr const HardCost& hard() const noexcept
    {
        return hard_;
    }

    [[nodiscard]]
    constexpr const SoftCost& soft() const noexcept
    {
        return soft_;
    }

    auto operator<=>(const hierarchical&) const = default;

    [[nodiscard]]
    friend constexpr long double operator-(
        const hierarchical& candidate,
        const hierarchical& current)
        requires has_delta<hierarchical>
    {
        return delta(candidate, current);
    }

private:
    HardCost hard_;
    SoftCost soft_;
};

/// Hard-preserving numeric delta. A namespace-scope function (not a hidden
/// friend) so that the qualified cost::delta(...) also finds it.
template<class HardCost, class SoftCost>
    requires requires(const HardCost& lhs, const HardCost& rhs) {
        { lhs < rhs } -> std::convertible_to<bool>;
        { lhs == rhs } -> std::convertible_to<bool>;
    } && has_delta<SoftCost>
[[nodiscard]]
constexpr long double delta(
    const hierarchical<HardCost, SoftCost>& candidate,
    const hierarchical<HardCost, SoftCost>& current)
{
    if (candidate.hard() < current.hard())
    {
        return -std::numeric_limits<long double>::infinity();
    }
    if (current.hard() < candidate.hard())
    {
        return std::numeric_limits<long double>::infinity();
    }
    if (candidate.hard() == current.hard())
    {
        return static_cast<long double>(
            delta(candidate.soft(), current.soft()));
    }

    // A hierarchical cost requires a total ordering of the hard branch for
    // a meaningful numeric delta. Conservatively make an unordered hard
    // transition unacceptable to delta-based algorithms.
    return std::numeric_limits<long double>::infinity();
}

template<class HardCost, class SoftCost>
    requires has_zero<HardCost> && has_zero<SoftCost>
struct zero_cost<hierarchical<HardCost, SoftCost>>
{
    [[nodiscard]]
    static constexpr hierarchical<HardCost, SoftCost> value()
    {
        return hierarchical<HardCost, SoftCost>{zero<HardCost>(), zero<SoftCost>()};
    }
};

template<class T>
struct is_hierarchical : std::false_type
{
};

template<class HardCost, class SoftCost>
struct is_hierarchical<hierarchical<HardCost, SoftCost>>
    : std::true_type
{
};

template<class T>
inline constexpr bool is_hierarchical_v =
    is_hierarchical<std::remove_cvref_t<T>>::value;

template<class T>
concept hierarchical_type = is_hierarchical_v<T>;

} // namespace easylocal::cost
