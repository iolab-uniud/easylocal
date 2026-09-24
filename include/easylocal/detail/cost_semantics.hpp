#pragma once

#include <concepts>

namespace easylocal::detail
{

// Cost semantics are contextual capabilities rather than requirements on the
// materialized cost_type itself. A SolutionManager may provide problem-specific
// semantics (for example maximization or tolerance-aware comparison). When it
// does not, the corresponding intrinsic cost operator is the exact/default
// fallback.
//
// Keep better, equivalent, and better_or_equivalent as independent queries.
// In particular, do not implement better_or_equivalent as better || equivalent:
// a future lazy cost model must be free to answer the <= semantic relation in a
// single pass over only the components needed to decide it.

template<class SM>
concept custom_better =
    requires(
        const SM& solution_manager,
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        {
            solution_manager.better(candidate, reference)
        } -> std::convertible_to<bool>;
    };

template<class SM>
concept intrinsic_better =
    requires(
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        { candidate < reference } -> std::convertible_to<bool>;
    };

template<class SM>
concept has_better = custom_better<SM> || intrinsic_better<SM>;

template<class SM>
    requires has_better<SM>
[[nodiscard]]
constexpr auto cost_better(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference) -> bool
{
    if constexpr (custom_better<SM>)
    {
        return static_cast<bool>(
            solution_manager.better(candidate, reference));
    }
    else
    {
        return static_cast<bool>(candidate < reference);
    }
}

template<class SM>
concept custom_equivalent =
    requires(
        const SM& solution_manager,
        const typename SM::cost_type& lhs,
        const typename SM::cost_type& rhs)
    {
        {
            solution_manager.equivalent(lhs, rhs)
        } -> std::convertible_to<bool>;
    };

template<class SM>
concept intrinsic_equivalent =
    requires(
        const typename SM::cost_type& lhs,
        const typename SM::cost_type& rhs)
    {
        { lhs == rhs } -> std::convertible_to<bool>;
    };

template<class SM>
concept has_equivalent = custom_equivalent<SM> || intrinsic_equivalent<SM>;

template<class SM>
    requires has_equivalent<SM>
[[nodiscard]]
constexpr auto cost_equivalent(
    const SM& solution_manager,
    const typename SM::cost_type& lhs,
    const typename SM::cost_type& rhs) -> bool
{
    if constexpr (custom_equivalent<SM>)
    {
        return static_cast<bool>(solution_manager.equivalent(lhs, rhs));
    }
    else
    {
        return static_cast<bool>(lhs == rhs);
    }
}

template<class SM>
concept custom_better_or_equivalent =
    requires(
        const SM& solution_manager,
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        {
            solution_manager.better_or_equivalent(candidate, reference)
        } -> std::convertible_to<bool>;
    };

template<class SM>
concept intrinsic_better_or_equivalent =
    requires(
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        { candidate <= reference } -> std::convertible_to<bool>;
    };

template<class SM>
concept has_better_or_equivalent =
    custom_better_or_equivalent<SM> || intrinsic_better_or_equivalent<SM>;

template<class SM>
    requires has_better_or_equivalent<SM>
[[nodiscard]]
constexpr auto cost_better_or_equivalent(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference) -> bool
{
    if constexpr (custom_better_or_equivalent<SM>)
    {
        return static_cast<bool>(
            solution_manager.better_or_equivalent(candidate, reference));
    }
    else
    {
        return static_cast<bool>(candidate <= reference);
    }
}

} // namespace easylocal::detail
