#pragma once

/// \file
/// Hierarchical hard/soft cost: the hard branch has strict priority, soft is
/// compared only when hard is equivalent.
///
/// Each branch may itself be scalar or structured (e.g. cost::lexicographic).
///
/// delta() preserves the hard level: hard better -> -inf, hard worse -> +inf,
/// hard equivalent -> soft delta. This lets delta-based acceptance work on the
/// full cost without ever accepting a hard degradation.

#include <easylocal/cost/concepts.hpp>

#include <compare>
#include <concepts>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

namespace detail
{

// A cost that compares with <=>, or else with < (both ways) alone, as
// std::tuple compares its elements.
template<class T>
concept synth_three_way_comparable =
    std::three_way_comparable<T> || requires(const T& lhs, const T& rhs) {
        { lhs < rhs } -> std::convertible_to<bool>;
    };

// The type of a hierarchical delta, which has the infinities of a change of
// the hard cost: double, or long double when the soft delta is one. A double
// keeps the Metropolis criterion off long double arithmetic, which is
// software on aarch64 Linux and x87 on x86-64.
template<class SoftCost>
using hierarchical_delta_t = std::conditional_t<
    std::same_as<
        std::remove_cvref_t<decltype(delta(
            std::declval<const SoftCost&>(),
            std::declval<const SoftCost&>()))>,
        long double>,
    long double,
    double>;

} // namespace detail

/// A hierarchical cost: the hard cost has strict priority, the soft cost is
/// compared only when the hard costs are equivalent (neither is less).
///
/// Each branch may itself be scalar or structured. Its `delta` never accepts a
/// hard degradation: minus infinity when the hard cost improves, plus infinity
/// when it worsens, the soft delta otherwise.
template<class HardCost, class SoftCost>
class hierarchical
{
    static_assert(
        !detail::unsigned_cost_v<HardCost> && !detail::unsigned_cost_v<SoftCost>,
        "a cost cannot be an unsigned integer, whose differences wrap "
        "around: use a signed integer (int, long long) or a floating-point "
        "type");

public:
    /// The type of the hard cost.
    using hard_cost_type = HardCost;
    /// The type of the soft cost.
    using soft_cost_type = SoftCost;

    /// From its hard and soft costs.
    constexpr explicit hierarchical(HardCost hard, SoftCost soft)
        : hard_{std::move(hard)},
          soft_{std::move(soft)}
    {
    }

    /// The hard cost.
    [[nodiscard]]
    constexpr const HardCost& hard() const noexcept
    {
        return hard_;
    }

    /// The soft cost.
    [[nodiscard]]
    constexpr const SoftCost& soft() const noexcept
    {
        return soft_;
    }

    /// The order of the costs: by the hard costs, then by the soft costs; each
    /// branch compares with its `<=>`, or else with its `<`.
    [[nodiscard]]
    friend constexpr auto operator<=>(const hierarchical& lhs, const hierarchical& rhs)
        requires detail::synth_three_way_comparable<HardCost>
        && detail::synth_three_way_comparable<SoftCost>
    {
        return std::tie(lhs.hard_, lhs.soft_) <=> std::tie(rhs.hard_, rhs.soft_);
    }

    /// Whether the hard and the soft costs are equal.
    [[nodiscard]]
    friend constexpr bool operator==(const hierarchical&, const hierarchical&) = default;

    /// `delta(candidate, current)`, when the soft cost has a delta.
    [[nodiscard]]
    friend constexpr auto operator-(
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

/// Hard-preserving numeric delta: minus infinity when the hard cost improves,
/// infinity when it worsens, the soft delta otherwise.
///
/// A double, or a long double when the soft delta is one. A namespace-scope
/// function (not a hidden friend) so that the qualified cost::delta(...) also
/// finds it.
template<class HardCost, class SoftCost>
    requires requires(const HardCost& lhs, const HardCost& rhs) {
        { lhs < rhs } -> std::convertible_to<bool>;
        { lhs == rhs } -> std::convertible_to<bool>;
    } && has_delta<SoftCost>
[[nodiscard]]
constexpr auto delta(
    const hierarchical<HardCost, SoftCost>& candidate,
    const hierarchical<HardCost, SoftCost>& current)
{
    using result = detail::hierarchical_delta_t<SoftCost>;
    if (candidate.hard() < current.hard())
    {
        return -std::numeric_limits<result>::infinity();
    }
    if (current.hard() < candidate.hard())
    {
        return std::numeric_limits<result>::infinity();
    }
    if (candidate.hard() == current.hard())
    {
        return static_cast<result>(delta(candidate.soft(), current.soft()));
    }

    // A hierarchical cost requires a total ordering of the hard branch for
    // a meaningful numeric delta. Conservatively make an unordered hard
    // transition unacceptable to delta-based algorithms.
    return std::numeric_limits<result>::infinity();
}

/// The zero of a hierarchical cost: the zero of the hard and of the soft cost.
template<class HardCost, class SoftCost>
    requires has_zero<HardCost> && has_zero<SoftCost>
struct zero_cost<hierarchical<HardCost, SoftCost>>
{
    /// The zero of both levels.
    [[nodiscard]]
    static constexpr hierarchical<HardCost, SoftCost> value()
    {
        return hierarchical<HardCost, SoftCost>{zero<HardCost>(), zero<SoftCost>()};
    }
};

/// Whether `T` is a `cost::hierarchical`.
template<class T>
struct is_hierarchical : std::false_type
{
};

/// A `cost::hierarchical` is one.
template<class HardCost, class SoftCost>
struct is_hierarchical<hierarchical<HardCost, SoftCost>>
    : std::true_type
{
};

/// Whether `T`, without cv and reference qualifiers, is a `cost::hierarchical`.
template<class T>
inline constexpr bool is_hierarchical_v =
    is_hierarchical<std::remove_cvref_t<T>>::value;

/// A `cost::hierarchical`, possibly cv- or reference-qualified.
template<class T>
concept hierarchical_type = is_hierarchical_v<T>;

} // namespace easylocal::cost
